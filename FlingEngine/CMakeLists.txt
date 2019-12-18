project( "FlingEngine" )

#### Setup Git version numbers ######
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Take care of warnings about strcpy
if( MSVC )
    add_definitions( -D_CRT_SECURE_NO_WARNINGS )
# GCC
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

    add_definitions( -Wno-class-memaccess )

# Clang
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

endif()


message( STATUS "Generated git file is : ${EngineDir}Core/inc/GitVersion_TEMPLATE.h" )

# Configure my generated files from cmake 
configure_file(
    Core/inc/GitVersion_TEMPLATE.h.in
    ${GENERATED_INC_FOLDER}/GitVersion.h
)

set ( FLING_ENGINE_SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" )

# Generate the asset path stuff 
configure_file(
    Resources/src/FlingAssetPaths.cpp.in
    ${GENERATED_INC_FOLDER}/FlingAssetPaths.cpp
)

# Include the Fling Engine source files that we may need
FLING_ENGINE_INC("")

##################### Linking #################

### Find Vulkan
find_package(Vulkan REQUIRED)

## Ensure we have Vulkan
if(Vulkan_FOUND)
    message(STATUS "Vulkan found!" )
else()
    message(FATAL_ERROR "Vulkan NOT FOUND! Stopping" )
endif()

if( WITH_LUA_FLAG )
	find_package(Lua53 REQUIRED)
	if(LUA53_FOUND)
		message(STATUS "Lua 5.3 found!")
	else()
		message(FATAL_ERROR "Lua 5.3 NOT FOUND! Stopping")
	endif()
endif()

set ( LINK_LIBS
    glfw ${GLFW_LIBRARIES}
    Vulkan::Vulkan  
    spirv-cross-core 
    spirv-cross-cpp
    spirv-cross-reflect
    ${BULLET_LIBRARIES}
)

IF( WITH_IMGUI_FLAG )
set ( LINK_LIBS ${LINK_LIBS}
    ImGui ${IMGUI_LIBRARIES}
)
endif()

if( WITH_LUA_FLAG )
set ( LINK_LIBS ${LINK_LIBS}
    ${LUA53_LIBRARIES}
)
endif()

# link pthread if we need to
if ( NOT WIN32 )
    set( LINK_LIBS ${LINK_LIBS} pthread )
    message( STATUS "Added pthread!" )
endif()

# we need to link filesystem on GCC 8+
if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
    set( LINK_LIBS ${LINK_LIBS} stdc++fs )
    message( STATUS "Added filesytem link!" )
endif()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set( LINK_LIBS ${LINK_LIBS} stdc++fs )
    message( STATUS "Added filesytem link (Clang)!" )
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set( LINK_LIBS ${LINK_LIBS} stdc++fs )
    message( STATUS "Added filesytem link (GNU)!" )
endif()

message( STATUS "LINK_LIBS is : " ${LINK_LIBS} )

################# Complier Options ############
if( MSVC )
    set ( MY_COMPILER_FLAGS "/W3" )
else()
    set ( MY_COMPILER_FLAGS "-Wall -Wno-reorder -Wno-unknown-pragmas -Wno-multichar -Wno-missing-braces" )
endif()

set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MY_COMPILER_FLAGS}" )

### Setup visual studio source groups / filters ###
file( GLOB_RECURSE _source_list
    *.cpp* src/*.h* src/*.hpp* *.h* ${GENERATED_INC_FOLDER}/*.h ${GENERATED_INC_FOLDER}/*.cpp *.inl
)

if( MSVC )
    foreach( _source IN ITEMS ${_source_list} )
    	get_filename_component( _source_path "${_source}" PATH )
        string( REPLACE "${CMAKE_SOURCE_DIR}" "" _group_path "${_source_path}" )
        string( REPLACE "/" "\\" _group_path "${_group_path}" )
        source_group( "${_group_path}" FILES "${_source}" )
    endforeach()
endif()

################# Add library and link ######################

add_library ( ${PROJECT_NAME} ${_source_list} )

# Make sure the compiler can find include files for our Engine library
target_include_directories (${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${SPIRV_CROSS_INCLUDE_DIR})

# link against the libs that the engine needs
target_link_libraries( ${PROJECT_NAME} LINK_PUBLIC ${LINK_LIBS} )