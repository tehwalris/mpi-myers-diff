#!/bin/bash -e
# from: https://scicomp.ethz.ch/wiki/FAQ#Environment_modules
DIR="$(dirname "$(readlink -f "$BASH_SOURCE")")"
SCRIPT_DIR=$DIR/../scripts/

# load new modulestack
source /cluster/apps/local/env2lmod.sh

echo "-> Loading modules required for build"
module list &> /dev/null || source /cluster/apps/modules/init/bash

module purge
module load gcc/8.2.0 openmpi/4.0.2 python/3.6.4
module list

# python setup warning
# automatic doesn't quite work
if [ ! -d "$SCRIPT_DIR/.venv/" ]; then
  # .venv doesn't exist
  tput setaf 1          # red
  printf "\n\nYou need to set up the Python virtual venv in ${SCRIPT_DIR}\n"
  tput sgr0             # reset
else
  echo "Activating venv"
  source "$SCRIPT_DIR/.venv/bin/activate"
fi
