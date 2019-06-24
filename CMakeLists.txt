cmake_minimum_required ( VERSION 3.13 )
set( CMAKE_SUPPRESS_REGENERATION true )

project ( "FlingEngine" )

# Include external libraries
include_directories( $ENV{VULKAN_INCLUDE} )
include_directories( external/glfw/include )
include_directories( external/glm )
include_directories( external/spdlog/include )


# Add the engine subdir
add_subdirectory ( "FlingEngine" )
