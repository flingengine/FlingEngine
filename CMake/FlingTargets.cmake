# Shared settings for first-party targets (FlingEngine, Sandbox, FlingTests,
# and every fling_add_module() target).

# Mirror the on-disk folder layout as Visual Studio filters.
function(fling_source_groups TARGET)
	if(NOT MSVC)
		return()
	endif()

	get_target_property(_sources ${TARGET} SOURCES)
	foreach(_source IN LISTS _sources)
		get_filename_component(_source_path "${_source}" PATH)
		string(REPLACE "${CMAKE_SOURCE_DIR}" "" _group_path "${_source_path}")
		string(REPLACE "/" "\\" _group_path "${_group_path}")
		source_group("${_group_path}" FILES "${_source}")
	endforeach()
endfunction()

# Warning level for first-party code. Third-party headers come in as SYSTEM
# includes (see FlingExternals.cmake), so these only apply to our own sources.
function(fling_set_warnings TARGET)
	if(MSVC)
		target_compile_options(${TARGET} PRIVATE /W3)
	else()
		target_compile_options(${TARGET} PRIVATE
			-Wall
			-Wno-reorder
			-Wno-unknown-pragmas
			-Wno-multichar
			-Wno-missing-braces
			-Wundef		# '#if FLING_LINUX' before Platform.h is included silently evaluates to 0
		)
		if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			target_compile_options(${TARGET} PRIVATE -Wno-class-memaccess)
		endif()
		# Std mutex types have no annotations. First-party code uses the wrappers
		# in Threading/, which Clang can check. A thread-safety warning fails the build.
		if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			target_compile_options(${TARGET} PRIVATE -Wthread-safety -Werror=thread-safety)
		endif()
	endif()
endfunction()

# Language standard, warnings, platform defines, and VS filters for a first-party target.
# Floating point rules for simulation code (physics, and later the fixed tick).
# The compiler must not fuse a multiply and an add into one FMA instruction, and
# must not use fast math, because both change the result on some platforms or
# compilers and break determinism. MSVC: /fp:precise does not fuse in VS 2022
# and later.
function(fling_set_deterministic_math TARGET)
	if(MSVC)
		target_compile_options(${TARGET} PRIVATE /fp:precise)
	else()
		target_compile_options(${TARGET} PRIVATE -ffp-contract=off -fno-fast-math)
	endif()
endfunction()

function(fling_configure_target TARGET)
	set_target_properties(${TARGET} PROPERTIES
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
	)

	if(MSVC)
		# NOMINMAX: Entt warning, see https://github.com/skypjack/entt/wiki/Frequently-Asked-Questions#warning-C4003-the-min-the-max-and-the-macro
		target_compile_definitions(${TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
	endif()

	fling_set_warnings(${TARGET})

	fling_source_groups(${TARGET})
endfunction()

# Executables land in <build>/<Target>/bin (plus /<Config> on multi-config
# generators); CI and FlingEngine.code-workspace depend on this layout.
function(fling_set_bin_output TARGET)
	set_target_properties(${TARGET} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
	)
endfunction()

# Checks a game's ECS system schedule after each build: runs the game headless
# with --validate-schedule, so a schedule error (two unordered writers, a cycle,
# a bad After name) fails `cmake --build` before anyone runs the game
# (Plans/ecs-systems.md section 3.6). Turn it off with -DFLING_VALIDATE_SCHEDULE=OFF.
option(FLING_VALIDATE_SCHEDULE "Check the ECS system schedule after building a game" ON)
function(fling_validate_schedule TARGET)
	if(NOT FLING_VALIDATE_SCHEDULE)
		return()
	endif()
	add_custom_command(TARGET ${TARGET} POST_BUILD
		COMMAND $<TARGET_FILE:${TARGET}> --null-render --unattended --validate-schedule
		WORKING_DIRECTORY "${FLING_ROOT_DIR}"
		COMMENT "Checking the ECS system schedule of ${TARGET}"
		VERBATIM
	)
endfunction()
