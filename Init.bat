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

:: Build GLFW in release and debug so that we can actually use it

::cd "external/glfw"
::cmake . -A x64
::cmake --build . --target ALL_BUILD --config Release
::cmake --build . --target ALL_BUILD --config Debug

:: Run cmake to generate the local x64 files in the build folder
::cd "../.."
cmake . -A x64 -B build