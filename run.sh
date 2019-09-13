#!/bin/bash
###############################################################################
# Author: Carlos Eduardo de Andrade <carlos.andrade@gatech.edu>
#
# (c) Copyright 2019
#     Georgia Institute of Technology.
#     All Rights Reserved.
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

function show_help_and_abort() {
    echo -e "$1 \nUsage: run.sh <seed> <instance> <results_dir>" > /dev/stderr
    exit 65 #NO_ARGS
}

SEED=$1
INSTANCE=$2
RESULTS_DIR=$3

if [ -z "$SEED" ] ; then
    show_help_and_abort "A non-empty seed is required."
fi

if ! [ -f "$INSTANCE" ]; then
    show_help_and_abort "A non-empty instance file is required."
fi

if ! [ -d "$RESULTS_DIR" ] ; then
    show_help_and_abort "A valid folder is required."
fi

###############################################################################

EXE="./brkga-feasibility-pump"

MAX_TIME="60"

CONFIG_FILE="brkga_tuned.conf"

#<stop-rule> <stop-arg>
#STOP_RULE="Generations 1000"
#STOP_RULE="Target 4020.32"
STOP_RULE="Iterations 1000"

#####################################
# FP config
#####################################

INITIAL_RELAXATIONS=1
STRATEGY="DefaultFP"
#STRATEGY="ObjFP"

#####################################
# OBJ section
#####################################

FITNESS_TYPE="Geometric"
#MIN_FACTOR=1.0      # Using only delta
MIN_FACTOR=0.0      # Using only number of frac vars
FACTOR_DECAY=0.9
DECAY_APPLIC=7

#####################################
# FP section
#####################################

FP_ITER_LIM=10
#FP_PERTURB="Perturb"
FP_PERTURB="NoPerturb"
FP_T=10
FP_RHO_LB=-0.2
FP_RHO_UB=0.5

#####################################
# OFP section
#####################################

OFP_PHI=0.85
OFP_DELTA=0.0001

#####################################
# Fixing section
#####################################

FIX_PERCENTAGE=0.10
FIX_RATE=0.05
FIX_STRATEGY="Zeros"
FIX_FREQUENCY=1

#####################################
# Rounding cuts
#####################################

ROUNDINGCUTS=0.0

#####################################
# MIP local search
#####################################

MIPLS_THRESHOLD=0.02
MIPLS_DISCREPANCY_LEVEL=0.05
MIPLS_UNFIXLEVELS=1
MIPLS_MAXTIME=60

#MIPLS_CTR_FILTERING="All"
MIPLS_CTR_FILTERING="Duals"
#MIPLS_CTR_FILTERING="SlacksAndDuals"

###############################################################################

$EXE $CONFIG_FILE $SEED $STOP_RULE $MAX_TIME \
    $INSTANCE $RESULTS_DIR \
    $INITIAL_RELAXATIONS $STRATEGY \
    OBJ $FITNESS_TYPE $MIN_FACTOR $FACTOR_DECAY $DECAY_APPLIC \
    FP $FP_ITER_LIM $FP_PERTURB $FP_T $FP_RHO_LB $FP_RHO_UB \
    OFP $OFP_PHI $OFP_DELTA \
    FIXING $FIX_PERCENTAGE $FIX_RATE $FIX_FREQUENCY $FIX_STRATEGY \
    ROUNDINGCUTS $ROUNDINGCUTS \
    MIPLOCALSEARCH $MIPLS_THRESHOLD $MIPLS_DISCREPANCY_LEVEL \
    $MIPLS_UNFIXLEVELS $MIPLS_MAXTIME $MIPLS_CTR_FILTERING

