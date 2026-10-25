cmake_minimum_required ( VERSION 3.21 )
set( CMAKE_SUPPRESS_REGENERATION true )
set( CMAKE_INSTALL_MESSAGE LAZY )   # Skips update to date messages

project ( "FlingEngine" VERSION 1.0 )

set ( FLING_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" )

# Let us use our *.cmake files now
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")
message( STATUS "Cmake mod path: " ${CMAKE_MODULE_PATH} )

# Include any .cmake macros here from the ./Cmake dir
include(FlingTargets)
include(FlingModule)
include (InstallRequiredSystemLibraries)

# Option for the current build configuration
OPTION( DEFINE_SHIPPING "DEFINE_SHIPPING configuration will change asset paths to be relative." OFF )

# Tests are off by default for shipping builds and when Fling is added to another project
if( PROJECT_IS_TOP_LEVEL AND NOT DEFINE_SHIPPING )
    set( _fling_tests_default ON )
else()
    set( _fling_tests_default OFF )
endif()
OPTION( FLING_BUILD_TESTS "Build the per-module Catch2 test executables (requires Catch2 3)" ${_fling_tests_default} )
OPTION( WITH_IMGUI_FLAG "WITH_IMGUI_FLAG will enable or disable the addition of IMGUI to the rendering pipeline. " ON )
OPTION( WITH_EDITOR_FLAG "Enables or disables the editor in the Fling Engine!" ON )
OPTION( WITH_CRASH_HANDLER_FLAG "Builds CrashHandler: --unattended crash handling and stack traces. Turn OFF to compile it out, for example when a shipping title has its own crash reporter" ON )
OPTION( ENABLE_MULTICORE "ENABLE_MULTICORE will allow MSVC to use all cores by adding the /MP option" ON)

# We can't have the editor without ImGUI!
IF( NOT WITH_IMGUI_FLAG AND WITH_EDITOR_FLAG )
    SET( WITH_EDITOR_FLAG OFF )
    message( FATAL_ERROR    "WITH_EDITOR_FLAG cannot be enabled because WITH_IMGUI_FLAG is OFF!" )
endif()

# EnTT 3.1 (external/entt) compares entities with entt::null through an operator==
# that calls itself forever under C++20 rewritten comparisons, unless the compiler
# implements P2468 (GCC 13, Clang 16). Older compilers build the engine, but any
# `entity == entt::null` then hangs at runtime, so stop here instead.
if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13 )
    message( FATAL_ERROR "Fling Engine needs GCC 13 or newer (found ${CMAKE_CXX_COMPILER_VERSION}). "
        "In C++20 mode it resolves EnTT 3.1's entt::null comparison to itself, which hangs." )
elseif( CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16 )
    message( FATAL_ERROR "Fling Engine needs Clang 16 or newer (found ${CMAKE_CXX_COMPILER_VERSION}). "
        "In C++20 mode it resolves EnTT 3.1's entt::null comparison to itself, which hangs." )
endif()

message( STATUS "----- Build Config ----" )
message( STATUS "Compiler=${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}" )
message( STATUS "WITH_EDITOR_FLAG=${WITH_EDITOR_FLAG}" )
message( STATUS "WITH_IMGUI_FLAG=${WITH_IMGUI_FLAG}" )
message( STATUS "WITH_CRASH_HANDLER_FLAG=${WITH_CRASH_HANDLER_FLAG}" )
message( STATUS "DEFINE_SHIPPING=${DEFINE_SHIPPING}" )
message( STATUS "ENABLE_MULTICORE=${ENABLE_MULTICORE}" )
message( STATUS "FLING_BUILD_TESTS=${FLING_BUILD_TESTS}" )
message( STATUS "FLING_ROOT_DIR=${FLING_ROOT_DIR}" )

# WITH_IMGUI / WITH_EDITOR / FLING_SHIPPING are set on the FlingEngine target
# (and propagate to its consumers) in FlingEngine/CMakeLists.txt.
IF( DEFINE_SHIPPING )
    message( STATUS "Build set to SHIPPING configuration!" )
    SET( CMAKE_BUILD_TYPE Release )
ENDIF( DEFINE_SHIPPING )
message( STATUS "-----------------------" )

# Default for third-party targets; first-party targets also get it from fling_configure_target()
set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

set( CMAKE_POSITION_INDEPENDENT_CODE ON )
set_property( GLOBAL PROPERTY USE_FOLDERS ON )

if( MSVC )
    # Enable multiple processor compilation
    if(ENABLE_MULTICORE)
        add_compile_options( /MP )
    endif()

    # spdlog 1.17+ bundles fmt v12, which requires UTF-8 source encoding on MSVC.
    # See https://github.com/fmtlib/fmt/pull/4159
    add_compile_options( /utf-8 )
endif()

find_package( Threads REQUIRED )

# Include external libraries
add_subdirectory( external/glfw )

# Consumers see GLFW's headers as system headers, so warnings inside them don't show up in our builds
get_target_property( _glfw_inc glfw INTERFACE_INCLUDE_DIRECTORIES )
set_target_properties( glfw PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_glfw_inc}" )

# SPIRV-Cross
set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)

add_subdirectory( external/SPIRV-Cross )
# Make the folders look pretty in Visual Studio
set_target_properties(spirv-cross-c PROPERTIES FOLDER SPIRV-Cross)
set_target_properties(spirv-cross-core PROPERTIES FOLDER SPIRV-Cross)
set_target_properties(spirv-cross-cpp PROPERTIES FOLDER SPIRV-Cross)
set_target_properties(spirv-cross-glsl PROPERTIES FOLDER SPIRV-Cross)
set_target_properties(spirv-cross-reflect PROPERTIES FOLDER SPIRV-Cross)
set_target_properties(spirv-cross-util PROPERTIES FOLDER SPIRV-Cross)

# Treat SPIRV-Cross headers as system headers for everything that links it, so
# warnings inside them don't show up in our own builds. The spirv.h/spirv.hpp
# headers trip MSVC C4065 (switch with only a 'default' label) - also mute that
# one inside SPIRV-Cross's own targets, since we never edit external/.
foreach( _spirv_target spirv-cross-c spirv-cross-core spirv-cross-cpp spirv-cross-glsl spirv-cross-reflect spirv-cross-util )
    get_target_property( _spirv_inc ${_spirv_target} INTERFACE_INCLUDE_DIRECTORIES )
    if( _spirv_inc )
        set_target_properties( ${_spirv_target} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_spirv_inc}" )
    endif()
    if( MSVC )
        target_compile_options( ${_spirv_target} PRIVATE /wd4065 )
    endif()
endforeach()

# ImGUI
# Built directly here (rather than via the submodule's own CMakeLists.txt) so that
# the submodule can stay pinned to an unmodified upstream ocornut/imgui tag.
if( WITH_IMGUI_FLAG )
    add_library( ImGui STATIC
        external/imgui/imgui.cpp
        external/imgui/imgui_demo.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp )
    target_include_directories( ImGui SYSTEM PUBLIC external/imgui )
    if( MSVC )
        target_compile_definitions( ImGui PRIVATE _CRT_SECURE_NO_WARNINGS )
    endif()
endif()

# Header-only libraries (FlingExt::glm, FlingExt::spdlog, ...)
include(FlingExternals)

# Add the engine
add_subdirectory ( "FlingEngine" )

# Tests are optional, so a build without them does not need Catch2
if( FLING_BUILD_TESTS )
    # Catch2 is installed locally by scripts/init.cmake (not system-wide) so find_package can locate it without sudo
    list( APPEND CMAKE_PREFIX_PATH "${FLING_ROOT_DIR}/external/Catch2/install" )
    enable_testing()
    add_subdirectory ( "FlingTests" )
endif()

# Add a subdirectory that uses the FlingEngine and produces an executeable
add_subdirectory ( "Sandbox" )
