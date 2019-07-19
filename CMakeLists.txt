cmake_minimum_required ( VERSION 3.13 )
set( CMAKE_SUPPRESS_REGENERATION true )
set( CMAKE_INSTALL_MESSAGE LAZY )   # Skips update to date messages

project ( "FlingEngine" )

# Print out what compilers are being used
message( STATUS "Using C   compiler:\t " ${CMAKE_C_COMPILER} )
message( STATUS "Using CXX compiler:\t " ${CMAKE_CXX_COMPILER} )

# Take care of warnings about strcpy
if( MSVC )
    add_definitions( -D_CRT_SECURE_NO_WARNINGS )
endif()

# Include external libraries
include_directories( $ENV{VULKAN_INCLUDE} )
add_subdirectory( external/glfw )
include_directories( external/glfw/include )

include_directories( external/glm )
include_directories( external/spdlog/include )
include_directories( external/inih )

# Add the engine and tests
add_subdirectory ( "FlingEngine" )
add_subdirectory ( "FlingTests" )

# Add a subdirectory that uses the FlingEngine and produces an executeable
add_subdirectory ( "Sandbox" )