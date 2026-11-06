# Script test (cmake -P): the provider search finds the engine providers and a
# provider outside the engine, with no engine change.
#   -DFLING_ROOT_DIR=<repo>

cmake_minimum_required(VERSION 3.21)

include("${FLING_ROOT_DIR}/CMake/FlingPhysicsProvider.cmake")

set(FLING_PHYSICS_PROVIDER_PATHS
	"${FLING_ROOT_DIR}/FlingEngine"
	"${FLING_ROOT_DIR}/FlingTests/PhysicsProviderExample"
)
fling_find_physics_providers()

foreach(_expected None Jolt Bullet Example)
	if(NOT _expected IN_LIST FLING_PHYSICS_PROVIDER_NAMES)
		message(FATAL_ERROR "Provider ${_expected} was not found. Found: ${FLING_PHYSICS_PROVIDER_NAMES}")
	endif()
endforeach()

if(NOT FLING_PHYSICS_PROVIDER_DIR_Example STREQUAL "${FLING_ROOT_DIR}/FlingTests/PhysicsProviderExample/Example")
	message(FATAL_ERROR "Example was found in the wrong folder: ${FLING_PHYSICS_PROVIDER_DIR_Example}")
endif()

message(STATUS "Physics providers found: ${FLING_PHYSICS_PROVIDER_NAMES}")
