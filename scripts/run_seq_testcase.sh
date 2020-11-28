#!/bin/bash

executable_name=own-diff-sequential.out

DIR="$(dirname "$(readlink -f "$BASH_SOURCE")")"


 if [ "$#" -eq  "1" ]
   then
      $DIR/../$executable_name $DIR/../test_cases/$1/in_1.txt  $DIR/../test_cases/$1/in_2.txt 
else
    echo "1st arg		the name of the test folder"
fi
