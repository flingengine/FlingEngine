cmake_minimum_required ( VERSION 3.13 )
set( CMAKE_SUPPRESS_REGENERATION true )

project ( "FlingEngine" )

# Include external libraries
#TODO: Repalce this with actually finding vulkan on the current machine
include_directories( C:/VulkanSDK/1.1.106.0/Include )
include_directories( external/glfw/include )
include_directories( external/glm )
include_directories( external/spdlog/include )


# Add the engine subdir
add_subdirectory ( "FlingEngine" )
