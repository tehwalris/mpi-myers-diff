#!/bin/bash

default_np=4
executable_name=own-diff-mpi.out

DIR="$(dirname "$(readlink -f "$BASH_SOURCE")")"

if [ "$#" -eq  "0" ]
   then	# no args
     echo "1st arg     		the name of the test folder"
     echo "2nd arg (optional) 	is the number of processes"
     exit 1
 elif [ "$#" -eq  "1" ]
   then
     mpiexec -np $default_np $DIR/../$executable_name $DIR/../test_cases/$1/in_1.txt  $DIR/../test_cases/$1/in_2.txt 
 else
     mpiexec -np $2 $DIR/../$executable_name $DIR/../test_cases/$1/in_1.txt  $DIR/../test_cases/$1/in_2.txt 
fi
