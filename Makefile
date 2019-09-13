###############################################################################
# Author: Carlos Eduardo de Andrade <ce.andrade@gmail.com>
#
# (c) Copyright 2015-2019
#     Industrial and Systems Engineering, Georgia Institute of Technology
#     All Rights Reserved.
#
# Created on : Dec 10, 2014 by andrade
# Last update: Ago 21, 2015 by andrade
#
# This code is released under LICENSE.md.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
###############################################################################

###############################################################################
# User Defines
###############################################################################

SYSTEM = macosx
#SYSTEM = linux

export SYSTEM

# Optimization switch
OPT = opt

# Set debug mode
#USER_DEFINES += -DDEBUG
#USER_DEFINES += -DFULLDEBUG
#USER_DEFINES += -DMIPDEBUG
#USER_DEFINES += -DEIGEN_DONT_PARALLELIZE
#USER_DEFINES += -DNO_TIGHTING_BOUNDS
#USER_DEFINES += -DNO_PRESOLVE

# Set tuning mode
#USER_DEFINES += -DTUNING

###############################################################################
# Build options
###############################################################################

BRKGA_EXE = brkga-feasibility-pump

BRKGA_MAIN = ./main_brkga.o

###############################
# Object files to multistart

MULTISTART_EXE = multistart-feasibility-pump
MULTISTART_MAIN = ./main_multistart.o

#########################
# The user source files
#########################

# Include dir
USER_INCDIRS = \
	./include \
	./brkga \
	./third_part	
	
# Source directories (to clean up object files)
SRC_DIRS = . \
	./decoders \
	./utils \
	./third_part/cluster
	
###############################
# Commom Object files
COMMON_OBJS = \
	./utils/execution_stopper.o

###############################
# Object files to BRKGA
BRKGA_OBJS = \
	./brkga/population.o \
	./decoders/feasibility_pump_decoder.o \
	./decoders/objective_feasibility_pump.o \
	./decoders/rounding_functions.o
	
###############################
# FP2.0 objects and stuff
FP2_INCDIRS = \
	./FP2 \
	./FP2/utils \
	./FP2/cpxutils \
	./FP2/propagator \
	./FP2/feaspump

FP2_LIBDIRS = ./FP2/obj
FP2_LIBS = -lfp -lprop -lcpxutils -lutils -lxml2 -llzma -lz

MODE=${OPT}
export MODE

###############################
# Clustering stuff
CLUSTER_INCDIRS = \
	./clustering

CLUSTER_OBJS = \
	./clustering/c_clustering_library/cluster.o \
	./clustering/cluster_tree.o \
	./clustering/clusterator.o

###############################################################################
# Compiler flags
###############################################################################

# Define the usage of C++11. This code compiles only in modern compilers,
# for instance, gcc >= 4.8
USER_FLAGS += -std=c++11

# Machine type
MACHINE = native

# Compiler flags for debugging
ifneq ($(OPT), opt)
	USER_FLAGS += -ggdb3 -fexceptions
else
	USER_FLAGS += -O3 -march=$(MACHINE) -mtune=$(MACHINE) \
		-ftracer -funroll-loops -fpeel-loops -fprefetch-loop-arrays
endif

# Disable AVX extensions since some CPUs are not capable to use them.
USER_FLAGS += -mno-avx

# Flags
USER_FLAGS += -Wall -Wextra -Wcast-align -Wcast-qual -Wdisabled-optimization \
				-Wformat=2 -Winit-self -Wmissing-format-attribute -Wshadow \
				-Wpointer-arith -Wredundant-decls -Wstrict-aliasing=2 \
				-Wfloat-equal -Weffc++

# Lemon has serious problems with this flag and GCC ignores the 
# "ignore" pragma.
#USER_FLAGS += -Winline
				
# Paranoid!!!
# Used to analyze format of structures 
#USER_FLAGS += -Wpacked -Wpadded

# Check conversions
#USER_FLAGS += -Wconversion
#USER_FLAGS += -Wunsafe-loop-optimizations # doesn't work very well with C++11 foreach

USER_FLAGS += -pthread -fopenmp 

###############################################################################
# Lib and include definitions
###############################################################################

ifeq ($(SYSTEM), macosx)
	INCLUDE_PATH = /opt/local
	USER_LIBDIRS = /opt/local/lib 
	USER_INCDIRS += /usr/local/include
endif

ifeq ($(SYSTEM), linux)
	INCLUDE_PATH = <put your include paths here>
	USER_LIBDIRS = <put your libs paths here>
endif

USER_INCDIRS += \
	$(INCLUDE_PATH)/include \
	$(INCLUDE_PATH)/include/boost

USER_LIBS += -lm

#Boost libraries
ifeq ($(OPT), opt)
	USER_FLAGS += -DBOOST_DISABLE_ASSERTS
endif

USER_DEFINES += -DBOOST_FILESYSTEM_NO_DEPRECATED

ifeq ($(SYSTEM), macosx)
	USER_LIBS += -lboost_system-mt -lboost_chrono-mt -lboost_timer-mt \
				 -lboost_filesystem-mt
else
	USER_LIBS += -lboost_system -lboost_chrono -lboost_timer -lboost_filesystem
endif


#############################
# Cplex and Concert settings
##############################

##############################
# Cplex and Concert settings
##############################

LIBFORMAT = static_pic

ifeq ($(SYSTEM), macosx)
	SYSTEM_DIR = x86-64_osx
	ILOG_DIR = ~/ILOG/CPLEX_Studio1262
endif

ifeq ($(SYSTEM), linux)
    SYSTEM_DIR = x86-64_linux
    ILOG_DIR = <put your CPLEX path here>
endif

CONCERTDIR = $(ILOG_DIR)/concert
CPLEXDIR = $(ILOG_DIR)/cplex
CPOPTIMIZERDIR = $(ILOG_DIR)/cpoptimizer

CPLEXINCDIRS = \
	$(CONCERTDIR)/include \
	$(CPLEXDIR)/include \
	$(CPOPTIMIZERDIR)/include

CPLEXLIBDIRS = \
	$(CONCERTDIR)/lib/$(SYSTEM_DIR)/$(LIBFORMAT) \
	$(CPLEXDIR)/lib/$(SYSTEM_DIR)/$(LIBFORMAT) \
	$(CPOPTIMIZERDIR)/lib/$(SYSTEM_DIR)/$(LIBFORMAT)

CPLEXLIBS = -lilocplex -lcplexdistmip -lcplex -lcp -lconcert -lm -lpthread -ldl

# For Mac OSX
ifeq ($(SYSTEM), macosx)
        CPLEXLIBS += -framework CoreFoundation -framework IOKit
endif

# Some flags
CPLEXFLAGS = -fPIC -fexceptions -DIL_STD -pthread

ifneq ($(findstring MIPDEBUG,$(USER_DEFINES)), MIPDEBUG)
        CPLEXFLAGS += -DNDEBUG
endif

###############################################################################
# Consolidate paths
###############################################################################

# Compiler flags
USER_FLAGS += $(CPLEXFLAGS)

# Libraries necessary to link.
LIBS += \
	$(CPLEXLIBS) \
	$(FP2_LIBS) \
	$(USER_LIBS)
	
# Necessary Include dirs
# Put -I in front of dirs
INCLUDES = `for i in $(USER_INCDIRS); do echo $$i | sed -e 's/^/-I/'; done`
INCLUDES += `for i in $(CPLEXINCDIRS); do echo $$i | sed -e 's/^/-I/'; done`
INCLUDES += `for i in $(FP2_INCDIRS); do echo $$i | sed -e 's/^/-I/'; done`
INCLUDES += `for i in $(CLUSTER_INCDIRS); do echo $$i | sed -e 's/^/-I/'; done`

# Necessary library dirs
# Put -L in front of dirs
LIBDIRS = `for i in $(USER_LIBDIRS); do echo $$i | sed -e 's/^/-L/'; done`
LIBDIRS += `for i in $(FP2_LIBDIRS); do echo $$i | sed -e 's/^/-L/'; done`
LIBDIRS += `for i in $(CPLEXLIBDIRS); do echo $$i | sed -e 's/^/-L/'; done`

###############################################################################
# Compiler and linker defs
###############################################################################

# C Compiler command and flags
CXX = g++
CXXFLAGS = $(USER_FLAGS)

# Linker
LD = ld

# Lib maker commands
AR = ar
ARFLAGS	= rv
RANLIB = ranlib

# Other includes
RM = rm
SHELL = /bin/bash

###############################################################################
# Build Rules
###############################################################################

.PHONY: all static clean doc docclean depclean
.SUFFIXES: .cpp .o

#all: brkga default_feasibility_pump multistart
all: brkga
#all: default_feasibility_pump
#all: multistart

static: brkga_static default_fp_static multistart_static

brkga: build_FP_lib $(CLUSTER_OBJS) $(COMMON_OBJS) $(BRKGA_OBJS) $(BRKGA_MAIN)
	@echo "--> Linking objects... "
	$(CXX) $(CXXFLAGS) $(COMMON_OBJS) $(CLUSTER_OBJS) $(BRKGA_OBJS) $(BRKGA_MAIN) -o $(BRKGA_EXE) $(LDFLAGS) $(LIBDIRS) $(LIBS) 
	@echo

brkga_static: build_FP_lib $(CLUSTER_OBJS) $(COMMON_OBJS) $(BRKGA_OBJS) $(BRKGA_MAIN)
	@echo "--> Linking objects... "
	$(CXX) -static -static-libgcc $(CXXFLAGS) $(COMMON_OBJS) $(CLUSTER_OBJS) $(BRKGA_OBJS) $(BRKGA_MAIN) -o $(BRKGA_EXE) \
	`for i in boost_system boost_timer boost_filesystem boost_chrono; do echo $(USER_LIBDIRS)/lib$${i}.a; done` \
	$(CONCERTDIR)/lib/$(SYSTEM_DIR)/$(LIBFORMAT)/libconcert.a \
	$(CPLEXDIR)/lib/$(SYSTEM_DIR)/$(LIBFORMAT)/libilocplex.a \
	$(CPLEXDIR)/lib/$(SYSTEM_DIR)/$(LIBFORMAT)/libcplex.a \
	$(CPLEXDIR)/lib/$(SYSTEM_DIR)/$(LIBFORMAT)/libcplexdistmip.a \
	-lm -pthread -fopenmp
	@echo

fix_and_local_search_random: build_FP_lib $(CLUSTER_OBJS) $(COMMON_OBJS) $(BRKGA_OBJS) fix_and_local_search_random.o
	@echo "--> Linking objects... "
	$(CXX) $(CXXFLAGS) $(COMMON_OBJS) $(CLUSTER_OBJS) $(BRKGA_OBJS) fix_and_local_search_random.o -o fix_and_local_search_random $(LDFLAGS) $(LIBDIRS) $(LIBS) 
	@echo

fix_and_local_search_relaxations: build_FP_lib $(CLUSTER_OBJS) $(COMMON_OBJS) $(BRKGA_OBJS) fix_and_local_search_relaxations.o
	@echo "--> Linking objects... "
	$(CXX) $(CXXFLAGS) $(COMMON_OBJS) $(CLUSTER_OBJS) $(BRKGA_OBJS) fix_and_local_search_relaxations.o -o fix_and_local_search_relaxations $(LDFLAGS) $(LIBDIRS) $(LIBS) 
	@echo

test_propagation: test_propagation.o
	@echo "--> Linking objects... "
	$(CXX) $(CXXFLAGS) test_propagation.o -o test_propagation $(LDFLAGS) $(LIBDIRS) $(LIBS) 
	@echo

test_fp2_propagator: build_FP_lib test_fp2_propagator.o	
	@echo "--> Linking objects... "
	$(CXX) $(CXXFLAGS) test_fp2_propagator.o -o test_fp2_propagator $(LDFLAGS) $(LIBDIRS) $(LIBS)
	@echo

test_clone: test_clone.o
	@echo "--> Linking objects... "
	$(CXX) $(CXXFLAGS) test_clone.o -o test_clone $(LDFLAGS) $(LIBDIRS) $(LIBS) 
	@echo

test_solution: build_FP_lib test_solution.o
	@echo "--> Linking objects... "
	$(CXX) $(CXXFLAGS) test_solution.o -o test_solution $(LDFLAGS) $(LIBDIRS) $(LIBS) 
	@echo

test_final_local_search: build_FP_lib $(CLUSTER_OBJS) $(COMMON_OBJS) $(BRKGA_OBJS) test_final_local_search.o
	@echo "--> Linking objects... "
	$(CXX) $(CXXFLAGS) $(COMMON_OBJS) $(CLUSTER_OBJS) $(BRKGA_OBJS) test_final_local_search.o -o test_final_local_search $(LDFLAGS) $(LIBDIRS) $(LIBS) 
	@echo

test_clustering: build_FP_lib $(CLUSTER_OBJS) test_clustering.o
	@echo "--> Linking objects... "
	$(CXX) $(CXXFLAGS) $(CLUSTER_OBJS) test_clustering.o -o test_clustering $(LDFLAGS) $(LIBDIRS) $(LIBS) 
	@echo

test_tree: build_FP_lib $(CLUSTER_OBJS) test_tree.o
	@echo "--> Linking objects... "
	$(CXX) $(CXXFLAGS) $(CLUSTER_OBJS) test_tree.o -o test_tree $(LDFLAGS) $(LIBDIRS) $(LIBS) 
	@echo

build_FP_lib:
	@echo "--> Building FP2.0 lib..."
	make -j2 -C FP2

.cpp.o:
	@echo "--> Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(USER_DEFINES) -c $< -o $@
	@echo

.c.o:
	@echo "--> Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(USER_DEFINES) -c $< -o $@
	@echo

codecheck:
	@echo "--> Checking for possible bugs..."
	cppcheck -ithird_part -iFP2 --enable=all .

doc:
	doxygen Doxyfile

clean:
	@echo "--> Cleaning compiled..."
	rm -rf $(COMMON_OBJS) $(CLUSTER_OBJS) $(BRKGA_OBJS) 
	rm -rf *o
	make -C FP2 clean
	
depclean: clean docclean
	rm -rf `for i in $(SRC_DIRS); do echo $$i*~; done` 
	rm -rf FP2/obj
	rm -rf Debug
	rm -rf $(BRKGA_EXE) $(DEFAULT_FP_EXE) fix_and_local_search_random \
		fix_and_local_search_relaxations test_propagation test_fp2_propagator \
		test_clone test_clustering test_solution test_final_local_search
		
docclean:
	@echo "--> Cleaning doc..."
	rm -rf doc

