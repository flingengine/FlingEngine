:: Fling Engine initalize script
:: Download any third party libs/API's and build them so that 
:: the devs can get to work faster! 

@echo off 

echo   =======================================================================
echo ================= Fling Engine initalize script ============================
echo   =======================================================================

:: Create the external/build folders if they are not created already
if not exist "external" mkdir external
if not exist "build" mkdir build

:: Clone any external libararies or other things that we are using 
echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
echo Cloning external repos...
echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

if not exist "external/glm"		git clone https://github.com/g-truc/glm.git		--recursive --quiet external/glm
if not exist "external/glfw"	git clone https://github.com/glfw/glfw.git		--recursive --quiet external/glfw
if not exist "external/spdlog"	git clone https://github.com/gabime/spdlog.git	--recursive --quiet external/spdlog
if not exist "external/inih"	git clone https://github.com/jtilly/inih.git	--recursive --quiet external/inih

:: Build GLFW in release and debug
echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
echo  Build GLFW 
echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

cd "external/glfw"
cmake . -A x64
cmake --build . --target ALL_BUILD --config Release
cmake --build . --target ALL_BUILD --config Debug

:: Run cmake to generate the local x64 files in the build folder
cd "../.."
cmake . -A x64 -B build
