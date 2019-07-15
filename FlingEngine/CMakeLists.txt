cmake_minimum_required ( VERSION 3.13 )

project( "FlingEngine" )

# Set Build Dir
set( CMAKE_BINARY_DIR    "/build" )
set_property( GLOBAL PROPERTY USE_FOLDERS ON )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/_Output/$<CONFIG> )
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_INSTALL_MESSAGE LAZY )   # Skips update to date messages

# Sets the output directory
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "bin" )

message( STATUS "Using compiler:\t " ${CMAKE_CXX_COMPILER} )

################### include folders ###########
include_directories(
  Core/inc
  Graphics/inc
  Utils/inc
  Resources/inc
)

##################### Linking #################
set( LINK_DIRS
    ../external/glfw/src/$<CONFIG>
    $ENV{VULKAN_LIB}
)

set ( LINK_LIBS
    glfw3.lib
    vulkan-1.lib
)

# link pthread if we need to
if ( NOT WIN32 )
    set( LINK_LIBS ${LINK_LIBS} pthread )
    message( STATUS "Added pthread!" )
endif()

message ( STATUS "LINK_DIRS: " ${LINK_DIRS} )

################# Complier Options ############
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

if( MSVC )
    foreach( _source IN ITEMS ${_source_list} )
    	get_filename_component( _source_path "${_source}" PATH )
        string( REPLACE "${CMAKE_SOURCE_DIR}" "" _group_path "${_source_path}" )
        string( REPLACE "/" "\\" _group_path "${_group_path}" )
        source_group( "${_group_path}" FILES "${_source}" )
    endforeach()
endif()

################# Add Exe and link ######################
message ( STATUS "\tcxx Flags:" ${CMAKE_CXX_FLAGS} )

link_directories( ${LINK_DIRS} )

add_library ( ${PROJECT_NAME} ${_source_list} )

# Make sure the compiler can find include files for our Engine library
target_include_directories (${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# link against the libs that the engine needs
target_link_libraries( ${PROJECT_NAME} LINK_PUBLIC ${LINK_LIBS} )