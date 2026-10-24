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

# Language standard, platform defines, and VS filters for a first-party target.
function(fling_configure_target TARGET)
	# TODO: switch to 20 once the warning cleanup lands; FLING_ENGINE_INC() was
	# silently forcing 17 on first-party code and the jump needs its own change.
	set_target_properties(${TARGET} PROPERTIES
		CXX_STANDARD 17
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
	)

	if(MSVC)
		# NOMINMAX: Entt warning, see https://github.com/skypjack/entt/wiki/Frequently-Asked-Questions#warning-C4003-the-min-the-max-and-the-macro
		target_compile_definitions(${TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
	endif()

	fling_source_groups(${TARGET})
endfunction()

# Executables land in <build>/<Target>/bin (plus /<Config> on multi-config
# generators); CI and FlingEngine.code-workspace depend on this layout.
function(fling_set_bin_output TARGET)
	set_target_properties(${TARGET} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
	)
endfunction()
