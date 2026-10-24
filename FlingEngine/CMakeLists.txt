
project ( "FlingEngine" VERSION 1.0 )

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


message( STATUS "Generated git file is : ${GENERATED_INC_FOLDER}/GitVersion.h" )

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
    Threads::Threads
    FlingExt::glm
    FlingExt::spdlog
    FlingExt::inih
    FlingExt::entt
    FlingExt::json
    FlingExt::stb
    FlingExt::tinyobjloader
    FlingExt::cereal
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
    *.cpp* src/*.h* src/*.hpp* *.h* ${GENERATED_INC_FOLDER}/*.h ${GENERATED_INC_FOLDER}/*.cpp *.inl
)

################# Add library and link ######################

add_library ( ${PROJECT_NAME} ${_source_list} )
fling_configure_target( ${PROJECT_NAME} )

# Make sure the compiler can find include files for our Engine library
target_include_directories( ${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Gameplay/inc
    ${GENERATED_INC_FOLDER}     # Generated include files that cmake will handle (i.e. GitVersion)
)

if( WITH_EDITOR_FLAG )
    target_include_directories( ${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Editor/inc )
endif()

# Feature flags are read from engine headers, so consumers need them too
target_compile_definitions( ${PROJECT_NAME} PUBLIC
    WITH_IMGUI=$<BOOL:${WITH_IMGUI_FLAG}>
    WITH_EDITOR=$<BOOL:${WITH_EDITOR_FLAG}>
    $<$<BOOL:${DEFINE_SHIPPING}>:FLING_SHIPPING>
)

# link against the libs that the engine needs
target_link_libraries( ${PROJECT_NAME} PUBLIC ${LINK_LIBS} )