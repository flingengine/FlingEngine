:: Fling Engine initalize script
:: Download any third party libs/API's and build them so that 
:: the devs can get to work faster! 

@echo off 

echo   =======================================================================
echo ================= Fling Engine initalize script ============================
echo   =======================================================================

:: Create the build folders if they are not created already
if not exist "build" mkdir build

:: Clone any external libararies or other things that we are using 
git submodule update --init --recursive

:: Run cmake to generate the local x64 files in the build folder
cmake . -G "Visual Studio 15 2017" -A x64 -B build
