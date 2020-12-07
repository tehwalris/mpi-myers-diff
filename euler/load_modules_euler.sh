#!/bin/bash
# from: https://scicomp.ethz.ch/wiki/FAQ#Environment_modules

echo "-> Loading modules required for build"
module list &> /dev/null || source /cluster/apps/modules/init/bash

module purge
module load legacy new gcc/4.8.2 open_mpi/1.6.5 python/3.6.0
