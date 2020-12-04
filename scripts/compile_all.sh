#!/bin/bash -e

DIR="$(dirname "$(readlink -f "$BASH_SOURCE")")"
cd $DIR/..

CFLAGS="-O3 -Wall -DNDEBUG"

printf "\nCompiling MPI Main:\n"
mpic++ src/main.cpp -o bin/own-diff-mpi-main.out $CFLAGS

printf "\nCompiling MPI No Master:\n"
mpic++ src/no_master_synch.cpp -o bin/own-diff-mpi-no-master.out $CFLAGS

printf "\nCompiling Sequential Fast Snakesn:\n"
g++ src/sequential_all_snakes_fast_lookup.cpp -o bin/own-diff-sequential-fast-snakes.out $CFLAGS

printf "\nCompiling Sequential:\n"
g++ src/sequential.cpp -o bin/own-diff-sequential.out $CFLAGS

