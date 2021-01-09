#!/bin/bash -e

DIR="$(dirname "$(readlink -f "$BASH_SOURCE")")"
cd $DIR/../diffutils

if [ ! -f ./configure ]; then
  git submodule update --init --recursive --progress
  ./bootstrap --gnulib-srcdir=gnulib --no-git
  ./configure CFLAGS="-w -O3 -march=znver2 -ffast-math"
fi

make
cp src/diff $DIR/../bin/diffutils.out
