
project ( "FlingEngine" VERSION 1.0 )

# Modules split out of the monolith so far (see Skills/module-split-plan.md)
add_subdirectory( Core )
add_subdirectory( Resources )
add_subdirectory( Gameplay )

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
    spirv-cross-core 
    spirv-cross-cpp
    spirv-cross-reflect
    Fling::Core
    Fling::Resources
    Fling::Gameplay
    FlingExt::stb
    FlingExt::tinyobjloader
)

IF( WITH_IMGUI_FLAG )
set ( LINK_LIBS ${LINK_LIBS}
    ImGui ${IMGUI_LIBRARIES}
)
endif()

# We need to link the std::filesystem on clang/GNU, but the windows version of
# Clang doesn't have this lib. GCC >= 9 folds std::filesystem into libstdc++ itself,
# so the standalone stdc++fs archive (which MinGW-w64/MSYS2 toolchains don't ship) is
# only needed on older GCC.
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    if(NOT WIN32)
    set( LINK_LIBS ${LINK_LIBS} stdc++fs )
        message( STATUS "Added filesytem link (Clang)!" )
    endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if( CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0 )
        set( LINK_LIBS ${LINK_LIBS} stdc++fs )
        message( STATUS "Added filesytem link (GNU)!" )
    endif()
endif()

message( STATUS "LINK_LIBS is : " ${LINK_LIBS} )

### Source files (VS filters are set up by fling_configure_target) ###
file( GLOB_RECURSE _source_list
    *.cpp* src/*.h* src/*.hpp* *.h* *.inl
)
# Module folders build as their own targets
list( FILTER _source_list EXCLUDE REGEX "^${CMAKE_CURRENT_SOURCE_DIR}/(Core|Resources|Gameplay)/" )

################# Add library and link ######################

add_library ( ${PROJECT_NAME} ${_source_list} )
fling_configure_target( ${PROJECT_NAME} )

# Make sure the compiler can find include files for our Engine library
target_include_directories( ${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/inc
)

if( WITH_EDITOR_FLAG )
    target_include_directories( ${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Editor/inc )
endif()

# Feature flags (WITH_IMGUI, WITH_EDITOR, FLING_SHIPPING) come from Fling::Core

# link against the libs that the engine needs
target_link_libraries( ${PROJECT_NAME} PUBLIC ${LINK_LIBS} )