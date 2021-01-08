#!/bin/bash

if [ -z "$1" ] 
then
    echo "Please provide a file name"
    exit 1
fi

sed "/^>/d" $1 | tr -d "\n" | sed "s/N//g; s/A/1\\n/g; s/C/2\\n/g; s/G/3\\n/g; s/T/4\\n/g"
