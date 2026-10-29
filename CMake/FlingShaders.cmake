# Fling shader helper.
#
# Compile GLSL shaders to SPIR-V as part of the build:
#
#   fling_add_shaders(Sandbox
#       BASE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
#       SOURCES      Shaders/Water.vert Shaders/Water.frag
#       INCLUDE_DIRS Shaders/common
#   )
#
# Each source compiles to ${FLING_SHADER_OUTPUT_DIR}/<path relative to BASE_DIR>.spv,
# so Shaders/Water.vert above becomes Water.vert.spv. Load it at runtime with
# Shader::Create(HS("Water.vert.spv"), ...). Two targets that write the same
# output path are a configure error.
#
# A shader recompiles when its source or a file it includes changes
# (glslangValidator --depfile). The engine shader folder (Assets/Shaders) is
# always on the include path, so any shader can #include "utils/Lights.h".
#
# The compiled shaders are a dependency of <target>, and are also built by the
# default "all" build. glslangValidator comes from the Vulkan SDK (or the
# glslang-tools package on Linux). Set FLING_GLSLANG_VALIDATOR to override it.

set(FLING_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Shaders" CACHE INTERNAL "Compiled SPIR-V output folder")
set(FLING_ENGINE_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Shaders" CACHE INTERNAL "Engine shader sources and shared shader headers")

function(_fling_find_glslang)
	if(FLING_GLSLANG_VALIDATOR)
		return()
	endif()

	find_program(FLING_GLSLANG_VALIDATOR
		NAMES glslangValidator
		HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
		DOC "glslangValidator used to compile shaders to SPIR-V"
	)

	if(NOT FLING_GLSLANG_VALIDATOR)
		message(FATAL_ERROR
			"glslangValidator was not found, so shaders cannot be compiled. "
			"Install the Vulkan SDK (Windows/macOS) or glslang-tools (Linux), "
			"or set FLING_GLSLANG_VALIDATOR to its path.")
	endif()
endfunction()

function(fling_add_shaders TARGET)
	cmake_parse_arguments(SHADERS
		""
		"BASE_DIR"
		"SOURCES;INCLUDE_DIRS"
		${ARGN}
	)

	if(NOT TARGET ${TARGET})
		message(FATAL_ERROR "fling_add_shaders(${TARGET}): target does not exist. Call this after creating it.")
	endif()
	if(NOT SHADERS_BASE_DIR)
		message(FATAL_ERROR "fling_add_shaders(${TARGET}) requires BASE_DIR")
	endif()
	if(NOT SHADERS_SOURCES)
		message(FATAL_ERROR "fling_add_shaders(${TARGET}) requires SOURCES")
	endif()

	_fling_find_glslang()

	get_filename_component(_base_dir "${SHADERS_BASE_DIR}" ABSOLUTE)

	set(_include_flags "-I${FLING_ENGINE_SHADER_DIR}")
	foreach(_inc IN LISTS SHADERS_INCLUDE_DIRS)
		get_filename_component(_inc "${_inc}" ABSOLUTE)
		list(APPEND _include_flags "-I${_inc}")
	endforeach()

	set(_sources)
	set(_outputs)
	foreach(_src IN LISTS SHADERS_SOURCES)
		get_filename_component(_src "${_src}" ABSOLUTE)
		file(RELATIVE_PATH _rel "${_base_dir}" "${_src}")
		if(_rel MATCHES "^\\.\\./")
			message(FATAL_ERROR "fling_add_shaders(${TARGET}): ${_src} is not under BASE_DIR ${_base_dir}")
		endif()

		set(_out "${FLING_SHADER_OUTPUT_DIR}/${_rel}.spv")
		set(_dep "${CMAKE_CURRENT_BINARY_DIR}/ShaderDeps/${_rel}.d")
		get_filename_component(_out_dir "${_out}" DIRECTORY)
		get_filename_component(_dep_dir "${_dep}" DIRECTORY)

		get_property(_owner GLOBAL PROPERTY "FLING_SHADER_OUTPUT_${_out}")
		if(_owner)
			message(FATAL_ERROR
				"fling_add_shaders(${TARGET}): ${_rel}.spv is already compiled by ${_owner}. "
				"Rename the shader or put it in a subfolder.")
		endif()
		set_property(GLOBAL PROPERTY "FLING_SHADER_OUTPUT_${_out}" "${TARGET}")

		add_custom_command(
			OUTPUT "${_out}"
			COMMAND "${CMAKE_COMMAND}" -E make_directory "${_out_dir}" "${_dep_dir}"
			COMMAND "${FLING_GLSLANG_VALIDATOR}" -V ${_include_flags} --depfile "${_dep}" -o "${_out}" "${_src}"
			DEPENDS "${_src}"
			DEPFILE "${_dep}"
			COMMENT "Compiling shader ${_rel}"
			VERBATIM
		)

		list(APPEND _sources "${_src}")
		list(APPEND _outputs "${_out}")
	endforeach()

	add_custom_target(${TARGET}Shaders ALL
		DEPENDS ${_outputs}
		SOURCES ${_sources}
	)
	source_group(TREE "${_base_dir}" PREFIX "Shaders" FILES ${_sources})
	set_target_properties(${TARGET}Shaders PROPERTIES FOLDER "Fling/Shaders")
	add_dependencies(${TARGET} ${TARGET}Shaders)
endfunction()
