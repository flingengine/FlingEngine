cmake_minimum_required ( VERSION 3.11.0 )
set( CMAKE_SUPPRESS_REGENERATION true )
set( CMAKE_INSTALL_MESSAGE LAZY )   # Skips update to date messages

project ( "FlingEngine" )

set ( FLING_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" )

# Option for seperate "shipping" config
OPTION( DEFINE_SHIPPING "DEFINE_SHIPPING configuration will change asset paths to be relative." OFF ) # disabled by default

IF( DEFINE_SHIPPING )
    message( STATUS "Build set to SHIPPING configuration!" )
    ADD_DEFINITIONS ( -DFLING_SHIPPING )
    SET( CMAKE_BUILD_TYPE Release )
ENDIF( DEFINE_SHIPPING )

# C++17 standard
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set(CMAKE_CXX_EXTENSIONS OFF)


# Generated folder used for Cmake generated files
set( GENERATED_INC_FOLDER ${CMAKE_BINARY_DIR}/Generated )
message( STATUS "Generated dir: ${GENERATED_INC_FOLDER}" )

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")
include(FlingEngineInc) # Could be in /tests/CMakeLists.txt
include(MSVC_PCH) # Could be in /tests/CMakeLists.txt

message( STATUS "Cmake mod path: " ${CMAKE_MODULE_PATH} )

# Take care of warnings about strcpy
if( MSVC )
    add_definitions( -D_CRT_SECURE_NO_WARNINGS )
	if ( NOT DEFINED NOMINMAX )
		add_definitions( -DNOMINMAX )	# Entt warning, see https://github.com/skypjack/entt/wiki/Frequently-Asked-Questions#warning-C4003-the-min-the-max-and-the-macro
	endif()
# GCC
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

    #add_definitions( -Wno-class-memaccess )
    add_definitions( -Wno-unused-private-field )
    add_definitions( -Wno-exceptions )
    add_definitions( -shared ) 
    add_definitions( -fPIC )

# Clang
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_definitions( -shared ) 
    add_definitions( -fPIC )
endif()


# -Wclass-memaccess

# This is needed for Clang support of glm::hash
ADD_DEFINITIONS( -DGLM_FORCE_CXX17 )

# Include external libraries
add_subdirectory( external/glfw )
include_directories( external/glfw/include )

add_subdirectory( external/imgui )
include_directories( external/imgui )

include_directories( external/glm )
include_directories( external/spdlog/include )
include_directories( external/inih )
include_directories( external/entt/src )
include_directories( external/json/single_include )
include_directories( external/stb )
include_directories( external/tinyobjloader )
include_directories( external/cereal/include )

# Add the engine and tests
add_subdirectory ( "FlingEngine" )
add_subdirectory ( "FlingTests" )

# Add a subdirectory that uses the FlingEngine and produces an executeable
add_subdirectory ( "Sandbox" )
