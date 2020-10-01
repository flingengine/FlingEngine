#!/bin/sh

echo Building Fling Engine in Release mode...

# Create the build folders if they are not created already
mkdir build -p

# Make sure that we have all external libraries that we need
git submodule update --init --recursive

# Run cmake!
cmake . -G "Visual Studio 16 2019" -T "clang-cl" -A x64 -B build
