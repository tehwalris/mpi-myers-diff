#!/bin/bash
# from: https://scicomp.ethz.ch/wiki/FAQ#Environment_modules

echo "-> Loading modules required for build"
module list &> /dev/null || source /cluster/apps/modules/init/bash

module purge
module load legacy new gcc/6.3.0 open_mpi/3.0.0 python/3.7.1
module list
