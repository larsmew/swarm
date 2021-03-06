# SWARM
#
# Copyright (C) 2012-2013 Torbjorn Rognes and Frederic Mahe
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
# 
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# Contact: Torbjorn Rognes <torognes@ifi.uio.no>, 
# Department of Informatics, University of Oslo, 
# PO Box 1080 Blindern, NO-0316 Oslo, Norway

# Makefile for SWARM

# Profiling options
#COMMON=-pg -g
COMMON=-g

COMPILEOPT=-Wall -mssse3 -mtune=core2

LIBS=-lpthread
LINKFLAGS=$(COMMON)

# Intel options
#CXX=icpc
#CXXFLAGS=$(COMPILEOPT) $(COMMON) -Wno-missing-declarations -fast

# GNU options
CXX=g++
CXXFLAGS=$(COMPILEOPT) $(COMMON) -O3

PROG=swarm

OBJS=swarm.o db.o search8.o search16.o nw.o matrix.o util.o scan.o algo.o qgram.o

DEPS=swarm.h Makefile


.SUFFIXES:.o .cc

%.o : %.cc $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.s : %.cc $(DEPS)
	$(CXX) $(CXXFLAGS) -c -S -o $@ $<

all : $(PROG)

swarm : $(OBJS)
	$(CXX) $(LINKFLAGS) -o $@ $(OBJS) $(LIBS)

clean :
	rm -f *.o *~ $(PROG) gmon.out
