#!/bin/bash -e

DIR="$(dirname "$(readlink -f "$BASH_SOURCE")")"
cd $DIR/..

CFLAGS="-std=c++17 -O3 -Wall -DNDEBUG -march=native -ffast-math"

printf "\nCompiling MPI Main:\n"
mpic++ src/main.cpp -o bin/own-diff-mpi-main.out $CFLAGS

printf "\nCompiling MPI No Master:\n"
mpic++ src/no_master_synch.cpp -o bin/own-diff-mpi-no-master.out $CFLAGS

printf "\nCompiling MPI Priority (FastStorage):\n"
mpic++ src/priority/main.cpp -o bin/own-diff-mpi-priority.out $CFLAGS

printf "\nCompiling MPI Priority (FrontierStorage):\n"
mpic++ src/priority/main.cpp -o bin/own-diff-mpi-priority-frontier.out -DFRONTIER_STORAGE $CFLAGS

printf "\nCompiling Sequential Fast Snakes:\n"
g++ src/sequential_all_snakes_fast_lookup.cpp -o bin/own-diff-sequential-fast-snakes.out $CFLAGS

printf "\nCompiling Sequential (FastStorage):\n"
g++ src/sequential.cpp -o bin/own-diff-sequential.out $CFLAGS

printf "\nCompiling Sequential (FrontierStorage):\n"
g++ src/sequential.cpp -o bin/own-diff-sequential-frontier.out -DFRONTIER_STORAGE $CFLAGS

