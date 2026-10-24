:: Fling Engine first-time setup. All of the logic lives in scripts/init.cmake
:: (shared with Init.sh); any args are passed through to it, e.g.
::   Init.bat -G Ninja -DCMAKE_BUILD_TYPE=Debug

@echo off
setlocal

where cmake >nul 2>nul
if errorlevel 1 (
	echo cmake was not found on PATH. Install CMake 3.21 or newer and run this again. 1>&2
	exit /b 1
)

pushd "%~dp0"
cmake -P scripts\init.cmake -- %*
set FLING_INIT_ERROR=%ERRORLEVEL%
popd
exit /b %FLING_INIT_ERROR%
