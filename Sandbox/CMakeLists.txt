cmake_minimum_required ( VERSION 3.13 )

project( "Sandbox" )

# Set Build Dir
set( CMAKE_BINARY_DIR    "/build" )
set_property( GLOBAL PROPERTY USE_FOLDERS ON )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/_Output/$<CONFIG> )
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_INSTALL_MESSAGE LAZY )   # Skips update to date messages

# Sets the output directory
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "bin" )

################### include folders ###########

set( ENGINE_DIR ../FlingEngine/ )

include_directories(
  src
  ${ENGINE_DIR}Core/inc
  ${ENGINE_DIR}Graphics/inc
  ${ENGINE_DIR}Resources/inc
  ${ENGINE_DIR}Utils/inc
)

##################### Linking #################

### Find Vulkan
find_package(Vulkan REQUIRED)

## Ensure we have Vulkan
if(Vulkan_FOUND)
    message(STATUS "Vulkan found!" )
else()
    message(FATAL_ERROR "Vulkan NOT FOUND! Stopping" )
endif()

set ( LINK_LIBS
    glfw ${GLFW_LIBRARIES}
    Vulkan::Vulkan  
	"FlingEngine"
)

# link pthread if we need to
if ( NOT WIN32 )
    set( LINK_LIBS ${LINK_LIBS} pthread )
    message( STATUS "Added pthread!" )
endif()

message( STATUS "LINK_LIBS is : " ${LINK_LIBS} )

################# Complier Options #################
if( MSVC )
    set ( MY_COMPILER_FLAGS "/W3" )
else()
    set ( MY_COMPILER_FLAGS "-Wall -Wno-reorder -Wno-unknown-pragmas -Wno-multichar" )
endif()

set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MY_COMPILER_FLAGS}" )

### Setup visual studio source groups / filters ###
file( GLOB_RECURSE _source_list
    *.cpp* src/*.h* src/*.hpp* *.h*
)

# Make visual studio look purdy
if( MSVC )
    foreach( _source IN ITEMS ${_source_list} )
    	get_filename_component( _source_path "${_source}" PATH )
        string( REPLACE "${CMAKE_SOURCE_DIR}" "" _group_path "${_source_path}" )
        string( REPLACE "/" "\\" _group_path "${_group_path}" )
        source_group( "${_group_path}" FILES "${_source}" )
    endforeach()
endif()

################# Add Exe and link ######################

# Link our normal libraries and create an executeable 
add_executable( ${PROJECT_NAME} ${_source_list} )

# Link libs
target_link_libraries( ${PROJECT_NAME} LINK_PUBLIC ${LINK_LIBS} )