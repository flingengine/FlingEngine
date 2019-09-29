#!/bin/sh

echo Building Fling Engine in Release mode...

# Create the build folders if they are not created already
mkdir build -p

# Make sure that we have all external libraries that we need
git submodule update --init --recursive

# Run cmake!
cmake . -B build -DCMAKE_BUILD_TYPE=Release -DDEFINE_SHIPPING=ON
