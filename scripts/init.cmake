# Fling Engine first-time setup, shared by Init.sh and Init.bat.
#
# Run from anywhere with:
#   cmake [-D<option>=<value>...] -P scripts/init.cmake [-- <args>...]
#
# Steps:
#   0. Make sure glslangValidator is installed (the build needs it for shaders)
#   1. Pull git submodules
#   2. Build and install Catch2 to external/Catch2/install (where the root
#      CMakeLists.txt looks for it), so the test executables can find it
#   3. Configure CMake into build/
#
# Options (as -D before -P, or as -DFLING_INIT_...=... after --):
#   FLING_INIT_CONFIGS          Catch2 configs to build (default "Debug;Release")
#   FLING_INIT_SKIP_SUBMODULES  ON skips step 1 (e.g. CI already checked them out)
#   FLING_INIT_SKIP_TESTS       ON skips step 2 and configures with FLING_BUILD_TESTS=OFF
#   FLING_INIT_SKIP_CONFIGURE   ON skips step 3
#
# Any other args after -- are passed to the build/ configure, e.g.
#   ./Init.sh -G Ninja -DCMAKE_BUILD_TYPE=Debug

cmake_minimum_required( VERSION 3.21 )

get_filename_component( FLING_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE )

# Split args after -- into FLING_INIT_ options and args for the build/ configure
set( _configure_args "" )
set( _past_separator OFF )
math( EXPR _last_arg "${CMAKE_ARGC} - 1" )
foreach( _i RANGE ${_last_arg} )
	set( _arg "${CMAKE_ARGV${_i}}" )
	if( NOT _past_separator )
		if( _arg STREQUAL "--" )
			set( _past_separator ON )
		endif()
	elseif( _arg MATCHES "^-D(FLING_INIT_[A-Z_]+)=(.*)$" )
		set( ${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" )
	else()
		list( APPEND _configure_args "${_arg}" )
	endif()
endforeach()

if( NOT DEFINED FLING_INIT_CONFIGS )
	set( FLING_INIT_CONFIGS "Debug;Release" )
endif()

# Runs a command from the repo root, echoing it first and stopping on failure
function( fling_init_run )
	list( JOIN ARGN " " _pretty )
	message( STATUS "> ${_pretty}" )
	execute_process(
		COMMAND ${ARGN}
		WORKING_DIRECTORY "${FLING_ROOT_DIR}"
		COMMAND_ERROR_IS_FATAL ANY
	)
endfunction()

message( "======================== Fling Engine init ========================" )

# ---------- Prerequisites ----------
# The build/ configure needs glslangValidator to compile shaders (see
# CMake/FlingShaders.cmake). Look for it now, so a missing tool stops init before
# the Catch2 build and not after it.
if( NOT FLING_INIT_SKIP_CONFIGURE )
	# A path given with -DFLING_GLSLANG_VALIDATOR=<path> is used as is
	foreach( _arg IN LISTS _configure_args )
		if( _arg MATCHES "^-DFLING_GLSLANG_VALIDATOR(:[A-Z]+)?=(.+)$" )
			set( _glslang_arg "${CMAKE_MATCH_2}" )
		endif()
	endforeach()

	# Message lines that start with spaces keep their layout; CMake rewraps the others
	if( _glslang_arg )
		if( NOT EXISTS "${_glslang_arg}" )
			message( FATAL_ERROR "FLING_GLSLANG_VALIDATOR is set to ${_glslang_arg}, but that file does not exist." )
		endif()
		set( _glslang "${_glslang_arg}" )
	else()
		find_program( _glslang
			NAMES glslangValidator
			HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
		)
		if( NOT _glslang )
			if( CMAKE_HOST_WIN32 OR CMAKE_HOST_APPLE )
				set( _hint
					"Install the Vulkan SDK from https://vulkan.lunarg.com/sdk/home. It includes glslangValidator. "
					"Then open a new terminal, so that the VULKAN_SDK variable that the installer sets is available, "
					"and run init again." )
			else()
				set( _hint
					"Install the glslang package for your distribution, for example:\n"
					"  sudo apt-get install glslang-tools   (Debian/Ubuntu)\n"
					"  sudo dnf install glslang             (Fedora)\n"
					"  sudo pacman -S glslang               (Arch)\n"
					"Or install the Vulkan SDK from https://vulkan.lunarg.com/sdk/home." )
			endif()
			if( DEFINED ENV{VULKAN_SDK} )
				list( APPEND _hint "\nVULKAN_SDK is set to $ENV{VULKAN_SDK}, but glslangValidator is not in its Bin folder." )
			endif()
			list( JOIN _hint "" _hint )
			message( FATAL_ERROR
				"glslangValidator was not found. The build needs it to compile shaders to SPIR-V.\n"
				"${_hint}\n"
				"If glslangValidator is installed in a different location, give its path to init:\n"
				"  Init -DFLING_GLSLANG_VALIDATOR=<path to glslangValidator>" )
		endif()
	endif()
	message( "-- Found glslangValidator: ${_glslang}" )
endif()

# ---------- 1. Submodules ----------
if( FLING_INIT_SKIP_SUBMODULES )
	message( "-- Skipping submodules (FLING_INIT_SKIP_SUBMODULES)" )
else()
	message( "-- Initializing submodules..." )
	find_program( GIT_EXECUTABLE git )
	if( NOT GIT_EXECUTABLE )
		message( FATAL_ERROR "git was not found on PATH. Install git and run this again." )
	endif()
	fling_init_run( "${GIT_EXECUTABLE}" submodule update --init --recursive )
endif()

# ---------- 2. Catch2 ----------
if( FLING_INIT_SKIP_TESTS )
	message( "-- Skipping Catch2 (FLING_INIT_SKIP_TESTS)" )
	list( APPEND _configure_args "-DFLING_BUILD_TESTS=OFF" )
else()
	set( _catch2_src "${FLING_ROOT_DIR}/external/Catch2" )
	set( _catch2_prefix "${_catch2_src}/install" )
	# One build dir per config works for single-config (Make/Ninja) and multi-config
	# (Visual Studio) generators alike. Debug libs get a "d" postfix, so every config
	# can share one install prefix.
	foreach( _config IN LISTS FLING_INIT_CONFIGS )
		message( "-- Installing Catch2 (${_config})..." )
		set( _catch2_build "${_catch2_src}/build-${_config}" )
		# --no-warn-unused-cli: multi-config generators ignore CMAKE_BUILD_TYPE
		fling_init_run( "${CMAKE_COMMAND}" -S "${_catch2_src}" -B "${_catch2_build}"
			--no-warn-unused-cli
			"-DCMAKE_BUILD_TYPE=${_config}"
			"-DCMAKE_INSTALL_PREFIX=${_catch2_prefix}"
			-DCMAKE_CXX_STANDARD=20
			-DCMAKE_CXX_STANDARD_REQUIRED=ON
			-DBUILD_TESTING=OFF
		)
		fling_init_run( "${CMAKE_COMMAND}" --build "${_catch2_build}" --config ${_config} --target install --parallel )
	endforeach()
endif()

# ---------- 3. Configure build/ ----------
if( FLING_INIT_SKIP_CONFIGURE )
	message( "-- Skipping build/ configure (FLING_INIT_SKIP_CONFIGURE)" )
else()
	message( "-- Configuring build/..." )
	fling_init_run( "${CMAKE_COMMAND}" -S "${FLING_ROOT_DIR}" -B "${FLING_ROOT_DIR}/build" ${_configure_args} )
	message( "" )
	message( "Done! Open FlingEngine.code-workspace in VS Code to build/run/debug" )
	message( "the engine, Sandbox, and per-module test executables from the Run and Debug menu." )
endif()
