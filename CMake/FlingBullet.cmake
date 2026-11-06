# Bullet physics (external/bullet3), Bullet 2 libraries only.
#
# We build LinearMath, BulletCollision, and BulletDynamics from source here, the
# same way as ImGui, and do not use Bullet's own CMakeLists.txt. Bullet's top-level
# project also builds demos, Bullet3, and OpenCL, and on MSVC it changes flags
# (/fp:fast, no RTTI, static runtime). Here the flags are the same as our own.
#
# Link FlingExt::bullet. Only the Physics module links it, and it links it PRIVATE,
# so Bullet headers stay out of every other module.

set(_bullet_src "${FLING_ROOT_DIR}/external/bullet3/src")

if(NOT EXISTS "${_bullet_src}/btBulletDynamicsCommon.h")
	message(FATAL_ERROR "external/bullet3 is missing. Run: git submodule update --init external/bullet3")
endif()

function(_fling_add_bullet_lib TARGET DIR)
	file(GLOB_RECURSE _sources CONFIGURE_DEPENDS "${_bullet_src}/${DIR}/*.cpp")
	add_library(${TARGET} STATIC ${_sources})
	target_include_directories(${TARGET} SYSTEM PUBLIC "${_bullet_src}")
	set_target_properties(${TARGET} PROPERTIES FOLDER Bullet)

	# Third-party code: do not show its warnings in our builds
	if(MSVC)
		target_compile_options(${TARGET} PRIVATE /W0)
		target_compile_definitions(${TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS)
	else()
		target_compile_options(${TARGET} PRIVATE -w)
	endif()
endfunction()

_fling_add_bullet_lib(fling_bullet_LinearMath LinearMath)
_fling_add_bullet_lib(fling_bullet_Collision BulletCollision)
_fling_add_bullet_lib(fling_bullet_Dynamics BulletDynamics)

target_link_libraries(fling_bullet_Collision PUBLIC fling_bullet_LinearMath)
target_link_libraries(fling_bullet_Dynamics PUBLIC fling_bullet_Collision)

add_library(fling_ext_bullet INTERFACE)
add_library(FlingExt::bullet ALIAS fling_ext_bullet)
target_link_libraries(fling_ext_bullet INTERFACE fling_bullet_Dynamics)

unset(_bullet_src)
