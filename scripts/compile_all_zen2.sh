#!/bin/bash -e

DIR="$(dirname "$(readlink -f "$BASH_SOURCE")")"
cd $DIR/..

CFLAGS="-std=c++17 -O3 -Wall -DNDEBUG -march=znver2 -ffast-math"

printf "\nCompiling MPI Master:\n"
mpic++ src/main.cpp -o bin/own-diff-mpi-master.out $CFLAGS

printf "\nCompiling MPI Master with SIMD:\n"
mpic++ src/main.cpp -o bin/own-diff-mpi-master-simd.out -DSIMD $CFLAGS

printf "\nCompiling MPI No Master (FastStorage):\n"
mpic++ src/no_master_synch.cpp -o bin/own-diff-mpi-no-master.out $CFLAGS

printf "\nCompiling MPI No Master (FastStorage) with SIMD:\n"
mpic++ src/no_master_synch.cpp -o bin/own-diff-mpi-no-master-simd.out -DSIMD $CFLAGS

printf "\nCompiling MPI No Master (FrontierStorage):\n"
mpic++ src/no_master_synch.cpp -o bin/own-diff-mpi-no-master-frontier.out -DFRONTIER_STORAGE $CFLAGS

printf "\nCompiling MPI No Master (FrontierStorage) with SIMD:\n"
mpic++ src/no_master_synch.cpp -o bin/own-diff-mpi-no-master-frontier-simd.out -DFRONTIER_STORAGE -DSIMD $CFLAGS

printf "\nCompiling MPI Priority (FastStorage):\n"
mpic++ src/priority/main.cpp -o bin/own-diff-mpi-priority.out $CFLAGS

printf "\nCompiling MPI Priority (FastStorage) with SIMD:\n"
mpic++ src/priority/main.cpp -o bin/own-diff-mpi-priority-simd.out -DSIMD $CFLAGS

printf "\nCompiling MPI Priority (FrontierStorage):\n"
mpic++ src/priority/main.cpp -o bin/own-diff-mpi-priority-frontier.out -DFRONTIER_STORAGE $CFLAGS

printf "\nCompiling MPI Priority (FrontierStorage) with SIMD:\n"
mpic++ src/priority/main.cpp -o bin/own-diff-mpi-priority-frontier-simd.out -DFRONTIER_STORAGE -DSIMD $CFLAGS

printf "\nCompiling Sequential Fast Snakes:\n"
g++ src/sequential_all_snakes_fast_lookup.cpp -o bin/own-diff-sequential-fast-snakes.out $CFLAGS

printf "\nCompiling Sequential (FastStorage):\n"
g++ src/sequential.cpp -o bin/own-diff-sequential.out $CFLAGS

printf "\nCompiling Sequential (FastStorage) with SIMD:\n"
g++ src/sequential.cpp -o bin/own-diff-sequential-simd.out -DSIMD $CFLAGS

printf "\nCompiling Sequential (FrontierStorage):\n"
g++ src/sequential.cpp -o bin/own-diff-sequential-frontier.out -DFRONTIER_STORAGE $CFLAGS

printf "\nCompiling Sequential (FrontierStorage) with SIMD:\n"
g++ src/sequential.cpp -o bin/own-diff-sequential-frontier-simd.out -DFRONTIER_STORAGE -DSIMD $CFLAGS

