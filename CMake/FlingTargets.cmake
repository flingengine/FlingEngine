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
	endif()
endfunction()

# Language standard, warnings, platform defines, and VS filters for a first-party target.
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
