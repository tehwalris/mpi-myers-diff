#!/bin/bash

DIR="$(dirname "$(readlink -f "$BASH_SOURCE")")"
cd $DIR/../diffutils

./configure CFLAGS="-w" && make
cp src/diff $DIR/../diffutils.out
