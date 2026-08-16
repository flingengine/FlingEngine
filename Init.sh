#!/bin/sh
echo   =============================================================
echo ================= Fling Engine initalize script =================
echo   =============================================================

# Make sure that we have all external libraries that we need
echo Initalizing submodules...
git submodule update --init --recursive

# Install Catch2 so that CMake can find it (installed locally so no sudo is needed)
echo Installing Catch2 testing library...
cmake ./external/Catch2 -B ./external/Catch2/build -DCMAKE_INSTALL_PREFIX=./external/Catch2/install
make install --directory=./external/Catch2/build -j$(nproc)

# Run cmake!
echo Running CMake 
cmake . -B build
