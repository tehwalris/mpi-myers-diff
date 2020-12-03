#!/bin/bash

executable_name=own-diff-sequential.out

DIR="$(dirname "$(readlink -f "$BASH_SOURCE")")"

if [ "$#" -eq  "0" ]
   then	# no args
     echo "1st arg     		the name of the test folder"
     exit 1
 else
     $DIR/../bin/$executable_name $DIR/../test_cases/$1/in_1.txt  $DIR/../test_cases/$1/in_2.txt 

fi
