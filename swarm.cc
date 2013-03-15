/*
    SWARM

    Copyright (C) 2012-2013 Torbjorn Rognes and Frederic Mahe

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Contact: Torbjorn Rognes <torognes@ifi.uio.no>,
    Department of Informatics, University of Oslo,
    PO Box 1080 Blindern, NO-0316 Oslo, Norway
*/

#include "swarm.h"

/* ARGUMENTS AND THEIR DEFAULTS */

#define DEFAULT_GAPOPEN 12
#define DEFAULT_GAPEXTEND 4
#define DEFAULT_MATCHSCORE 5
#define DEFAULT_MISMATCHSCORE (-4)
#define DEFAULT_THREADS 1
#define DEFAULT_RESOLUTION 1

char * outfilename;
char * statsfilename;
char * uclustfilename;
char * progname;
char * databasename;
long gapopen;
long gapextend;
long matchscore;
long mismatchscore;
long threads;
long resolution;

long penalty_factor;
long penalty_gapextend;
long penalty_gapopen;
long penalty_mismatch;

/* related to MPI */
#ifdef MPISWARM
#define MPI_MASTER 0
int mpirank,mpisize;
int mpithreads;
#endif

/* Other variables */

long mmx_present;
long sse_present;
long sse2_present;
long sse3_present;
long ssse3_present;
long sse41_present;
long sse42_present;
long popcnt_present;
long avx_present;

unsigned long dbsequencecount = 0;

FILE * outfile;
FILE * statsfile;
FILE * uclustfile;

char sym_nt[] = "-acgt                           ";

#define cpuid(f,a,b,c,d)						\
  __asm__ __volatile__ ("cpuid":					\
			"=a" (a), "=b" (b), "=c" (c), "=d" (d) : "a" (f));

void cpu_features_detect()
{
  unsigned int a __attribute__ ((unused));
  unsigned int b __attribute__ ((unused));
  unsigned int c, d;

  cpuid(1,a,b,c,d);

  mmx_present    = (d >> 23) & 1;
  sse_present    = (d >> 25) & 1;
  sse2_present   = (d >> 26) & 1;
  sse3_present   = (c >>  0) & 1;
  ssse3_present  = (c >>  9) & 1;
  sse41_present  = (c >> 19) & 1;
  sse42_present  = (c >> 20) & 1;
  popcnt_present = (c >> 23) & 1;
  avx_present    = (c >> 28) & 1;
}

void cpu_features_show()
{
  fprintf(stderr, "CPU features:     ");
  if (mmx_present)
    fprintf(stderr, " mmx");
  if (sse_present)
    fprintf(stderr, " sse");
  if (sse2_present)
    fprintf(stderr, " sse2");
  if (sse3_present)
    fprintf(stderr, " sse3");
  if (ssse3_present)
    fprintf(stderr, " ssse3");
  if (sse41_present)
    fprintf(stderr, " sse4.1");
  if (sse42_present)
    fprintf(stderr, " sse4.2");
  if (popcnt_present)
    fprintf(stderr, " popcnt");
  if (avx_present)
    fprintf(stderr, " avx");
  fprintf(stderr, "\n");
}


void args_getstring(int i, int argc, char **argv, char ** result, char * error)
{
  if (i+1 < argc)
    *result = argv[i+1];
  else
    fatal(error);
}

void args_getnum(int i, int argc, char **argv, long * result, char * error)
{
  if (i+1 < argc)
    *result = atol(argv[i+1]);
  else
    fatal(error);
}

void args_show()
{
  cpu_features_show();
  fprintf(stderr, "Input data file:   %s\n", databasename ? databasename : "(stdin)");
  fprintf(stderr, "Input data size:   %ld nucleotides", db_getnucleotidecount());
  fprintf(stderr, " in %ld sequences\n", db_getsequencecount());
  fprintf(stderr, "Longest sequence:  %ld nucleotides\n", db_getlongestsequence());
  fprintf(stderr, "Output file:       %s\n", outfilename ? outfilename : "(stdout)");
  if (statsfilename)
    fprintf(stderr, "Statistics file:   %s\n", statsfilename);
  fprintf(stderr, "Differences:       %ld\n", resolution);
  fprintf(stderr, "Threads:           %ld\n", threads);
#ifdef MPISWARM
  fprintf(stderr, "Total number of Threads: %ld\n", mpithreads);
#endif
  fprintf(stderr, "Match score:       %ld\n", matchscore);
  fprintf(stderr, "Mismatch penalty:  %ld\n", -mismatchscore);
  fprintf(stderr, "Gap penalties:     opening: %ld, extension: %ld\n", gapopen, gapextend);
  fprintf(stderr, "Converted costs:   factor: %ld, mismatch: %ld, gap opening: %ld, gap extension: %ld\n",
	  penalty_factor, penalty_mismatch, penalty_gapopen, penalty_gapextend);
}

void args_usage()
{
  /*               0         1         2         3         4         5         6         7          */
  /*               01234567890123456789012345678901234567890123456789012345678901234567890123456789 */

  fprintf(stderr, "Usage: %s [OPTIONS] [filename]\n", progname);
  fprintf(stderr, "  -d, --differences INTEGER           resolution (1)\n");
  fprintf(stderr, "  -h, --help                          display this help and exit\n");
  fprintf(stderr, "  -o, --output-file FILENAME          output result filename (stdout)\n");
  fprintf(stderr, "  -t, --threads INTEGER               number of threads to use (1)\n");
  fprintf(stderr, "  -v, --version                       display version information and exit\n");
  fprintf(stderr, "  -m, --match-reward INTEGER          reward for nucleotide match (5)\n");
  fprintf(stderr, "  -p, --mismatch-penalty INTEGER      penalty for nucleotide mismatch (4)\n");
  fprintf(stderr, "  -g, --gap-opening-penalty INTEGER   gap open penalty (12)\n");
  fprintf(stderr, "  -e, --gap-extension-penalty INTEGER gap extension penalty (4)\n");
  fprintf(stderr, "  -s, --statistics-file FILENAME      dump swarm statistics to file (no)\n");
  fprintf(stderr, "  -u, --uclust-file FILENAME          output in UCLUST-like format to file (no)\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "See 'man swarm' for more details.\n");
}

void show_header()
{
  char title[] = "SWARM " SWARM_VERSION;
  char ref[] = "Copyright (C) 2012-2013 Torbjorn Rognes and Frederic Mahe";
  fprintf(stderr, "%s [%s %s]\n%s\n\n", title, __DATE__, __TIME__, ref);
}

void args_init(int argc, char **argv)
{
  /* Set defaults */

  progname = argv[0];

  databasename = NULL;
  outfilename = NULL;
  statsfilename = NULL;
  resolution = DEFAULT_RESOLUTION;
  threads = DEFAULT_THREADS;
  matchscore = DEFAULT_MATCHSCORE;
  mismatchscore = DEFAULT_MISMATCHSCORE;
  gapopen = DEFAULT_GAPOPEN;
  gapextend = DEFAULT_GAPEXTEND;
  
  opterr = 1;

  char short_options[] = "d:ho:t:vm:p:g:e:s:u:";

  static struct option long_options[] =
  {
    {"differences",           required_argument, NULL, 'd' },
    {"help",                  no_argument,       NULL, 'h' },
    {"output-file",           required_argument, NULL, 'o' },
    {"threads",               required_argument, NULL, 't' },
    {"version",               no_argument,       NULL, 'v' },
    {"match-reward",          required_argument, NULL, 'm' },
    {"mismatch-penalty",      required_argument, NULL, 'p' },
    {"gap-opening-penalty",   required_argument, NULL, 'g' },
    {"gap-extension-penalty", required_argument, NULL, 'e' },
    {"statistics-file",       required_argument, NULL, 's' },
    {"uclust-file",           required_argument, NULL, 'u' },
    { 0, 0, 0, 0 }
  };
  
  int option_index = 0;
  int c;
  
  while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1)
  {
    switch(c)
    {
    case 'd':
      /* differences (resolution) */
      resolution = atol(optarg);
      break;
	  
    case 'o':
      /* output-file */
      outfilename = optarg;
      break;
	  
    case 't':
      /* threads */
      threads = atol(optarg);
      break;
	  
    case 'v':
      /* version */
#ifdef MPISWARM
      if(mpirank==MPI_MASTER) 
#endif
      {
       show_header();
      }
      exit(1);
      break;

    case 'm':
      /* match reward */
      matchscore = atol(optarg);
      break;
	  
    case 'p':
      /* mismatch penalty */
      mismatchscore = - atol(optarg);
      break;
	  
    case 'g':
      /* gap opening penalty */
      gapopen = atol(optarg);
      break;
	  
    case 'e':
      /* gap extension penalty */
      gapextend = atol(optarg);
      break;
	  
    case 's':
      /* statistics-file */
      statsfilename = optarg;
      break;
	  
    case 'u':
      /* uclust-file */
      uclustfilename = optarg;
      break;
	  
    case 'h':
      /* help */
    default:
      args_usage();
      exit(1);
      break;
    }
  }
  
  if (optind < argc)
    databasename = argv[optind];
  
  if (resolution < 1)
    fatal("Illegal resolution specified.");

  if ((threads < 1) || (threads > MAX_THREADS))
    fatal("Illegal number of threads specified");
  
  if ((gapopen < 0) || (gapextend < 0) || ((gapopen + gapextend) < 1))
    fatal("Illegal gap penalties specified.");

  if (matchscore < 1)
    fatal("Illegal match reward specified.");

  if (mismatchscore > -1)
    fatal("Illegal mismatch penalty specified.");

  if (outfilename)
    {
#ifdef MPISWARM
  if(mpirank==MPI_MASTER) 
#endif
    {
      outfile = fopen(outfilename, "w");
      if (! outfile)
	fatal("Unable to open output file for writing.");
    }
   }
  else
    outfile = stdout;
  
  if (statsfilename)
    {
#ifdef MPISWARM
  if(mpirank==MPI_MASTER) 
#endif
     {
      statsfile = fopen(statsfilename, "w");
      if (! statsfile)
	fatal("Unable to open statistics file for writing.");
     }
    }
  else
    statsfile = 0;
  
  if (uclustfilename)
    {
#ifdef MPISWARM
  if(mpirank==MPI_MASTER) 
#endif
     {
      uclustfile = fopen(uclustfilename, "w");
      if (! uclustfile)
	fatal("Unable to open uclust file for writing.");
     }
    }
  else
    uclustfile = 0;
  
}

int main(int argc, char** argv)
{
#ifdef MPISWARM
  int rc = MPI_Init(&argc, &argv);
  if (rc != MPI_SUCCESS)
    fatal("Unable to initialize MPI.");
  MPI_Comm_rank(MPI_COMM_WORLD, &mpirank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpisize);
  cpu_features_detect();
#else
  cpu_features_detect();

  if (! sse41_present)
    fprintf(stderr, "Warning: This program requires a processor with SSE4.1.\n");
#endif

  args_init(argc, argv);
#ifdef MPISWARM
  mpithreads=mpisize*threads;
#endif

  penalty_mismatch = matchscore - mismatchscore;
  penalty_gapopen = gapopen;
  penalty_gapextend = matchscore + gapextend;

  penalty_factor = gcd(gcd(penalty_mismatch, penalty_gapopen), penalty_gapextend);
  
  penalty_mismatch /= penalty_factor;
  penalty_gapopen /= penalty_factor;
  penalty_gapextend /= penalty_factor;

#ifdef MPISWARM
  if(mpirank==MPI_MASTER) 
#endif
  {
   show_header();
  }
  
#ifdef MPISWARM
  // TODO: Only one Task per node should read and in case of massive
  // parallelism this should be reduced further
#endif
  db_read(databasename);

  dbsequencecount = db_getsequencecount();

#ifdef MPISWARM
  if(mpirank==MPI_MASTER) 
#endif
  {
   args_show();
  }

  score_matrix_init();

  search_begin();
  
  algo_run();

  search_end();

  score_matrix_free();

  db_free();

  if (uclustfile)
    fclose(uclustfile);

  if (statsfile)
    fclose(statsfile);

  if (outfilename)
    fclose(outfile);
}
