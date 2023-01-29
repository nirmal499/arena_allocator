#! /usr/bin/bash

if [ $# -eq 0 ]; then
    echo "Need arguments"
    exit 1
fi

if [ $1 -eq 1 ]; then
    # Configure
    cmake -S . -B build/
elif [ $1 -eq 2 ]; then
    # Build
    cmake --build build/
elif [ $1 -eq 3 ]; then
    # Run
    ./build/src/main
else
    echo "Invalid value provided"
fi