#!/bin/sh

# Fling Engine initalize script

echo   =======================================================================
echo ================= Fling Engine initalize script ============================
echo   =======================================================================

# Create the build folders if they are not created already
mkdir build -p

# Make sure that we have all external libraries that we need
git submodule update --init --recursive

# Build GLFW in debuf and release mode
cd "external/glfw"
cmake . -A x64
cmake --build . --target ALL_BUILD --config Release
cmake --build . --target ALL_BUILD --config Debug

# Run cmake to generate the local x64 files in the build folder
cd "../.."
cmake . -A x64 -B build