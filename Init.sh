#!/bin/sh
echo   =============================================================
echo ================= Fling Engine initalize script =================
echo   =============================================================

# Create the build folders if they are not created already
mkdir build -p

# Make sure that we have all external libraries that we need
git submodule update --init --recursive

# Run cmake!
cmake . -B build