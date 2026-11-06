# Jolt Physics (external/JoltPhysics, our fork at a release tag).
#
# Uses Jolt's own Build/CMakeLists.txt, which is the supported way to embed it.
# The settings below are set before it is added. Jolt sets its compiler flags
# for its own folder only, so they do not reach our targets.
#
# Link FlingExt::jolt. Only the PhysicsJolt module links it, and it links it
# PRIVATE, so Jolt headers stay out of every other module.

set(_jolt_root "${FLING_ROOT_DIR}/external/JoltPhysics")

if(NOT EXISTS "${_jolt_root}/Build/CMakeLists.txt")
	message(FATAL_ERROR "external/JoltPhysics is missing. Run: git submodule update --init external/JoltPhysics")
endif()

# Same result on every platform and compiler. This also turns off FMA in Jolt.
set(CROSS_PLATFORM_DETERMINISTIC ON CACHE BOOL "" FORCE)

# Our targets use the shared MSVC runtime (/MD). Jolt's default is the static one.
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)

# Jolt's GPU compute backends. We do not use them, and the Vulkan one would make
# the game link the Vulkan import library (see the "no Vulkan import" test).
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)

# Keep our own build settings: no warnings-as-errors in third-party code, no
# link-time optimization, and our Debug/Release flags.
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)

# Our code is built with RTTI. PhysicsJolt subclasses Jolt types (JoltJobSystem),
# which then needs Jolt's type info, so Jolt is built with RTTI too.
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)

# Features we do not use yet
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
set(JPH_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory("${_jolt_root}/Build" "${CMAKE_BINARY_DIR}/external/JoltPhysics" EXCLUDE_FROM_ALL)
set_target_properties(Jolt PROPERTIES FOLDER Jolt)

# Consumers see Jolt's headers as system headers, so warnings inside them don't show up in our builds
get_target_property(_jolt_inc Jolt INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(Jolt PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_jolt_inc}")

add_library(fling_ext_jolt INTERFACE)
add_library(FlingExt::jolt ALIAS fling_ext_jolt)
target_link_libraries(fling_ext_jolt INTERFACE Jolt)

unset(_jolt_inc)
unset(_jolt_root)
