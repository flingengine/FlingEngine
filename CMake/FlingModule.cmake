# Fling build-module helper.
#
# Add a new engine module with a folder (inc/ + src/) and:
#
#   fling_add_module(Physics
#       PUBLIC_DEPS  Core
#       PRIVATE_DEPS Gameplay
#   )
#
# Public deps propagate include dirs and link libs to consumers.
# Private deps do not. Modules are SHARED by default; pass STATIC for a
# static library (API macros become empty).
#
# Generated ${NAME}API.h (e.g. GraphicsAPI.h / GRAPHICS_API) is written to
# that module's build dir and is PUBLIC, so only linkers of the module see it.
#
# Do not call this until the module's PUBLIC_DEPS / PRIVATE_DEPS targets
# already exist (bottom-up add_subdirectory order). Cycles are a configure error.

set(FLING_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "Path to first-party CMake helpers")

function(fling_add_module NAME)
	cmake_parse_arguments(MODULE
		"STATIC"
		""
		"PUBLIC_DEPS;PRIVATE_DEPS;PUBLIC_LIBS;PRIVATE_LIBS"
		${ARGN}
	)

	if(NOT NAME)
		message(FATAL_ERROR "fling_add_module() requires a module name")
	endif()

	string(TOUPPER "${NAME}" MODULE_UPPER)

	foreach(dep IN LISTS MODULE_PUBLIC_DEPS MODULE_PRIVATE_DEPS)
		if(NOT TARGET "${dep}" AND NOT TARGET "Fling::${dep}")
			message(FATAL_ERROR
				"fling_add_module(${NAME}): dependency '${dep}' does not exist. "
				"Add modules bottom-up (dependencies before dependents).")
		endif()

		get_property(_dep_all GLOBAL PROPERTY "FLING_MODULE_${dep}_ALL_DEPS")
		if(NAME IN_LIST _dep_all)
			message(FATAL_ERROR
				"Fling module dependency cycle: ${NAME} -> ${dep} -> ... -> ${NAME}")
		endif()
	endforeach()

	set(_all_deps ${MODULE_PUBLIC_DEPS} ${MODULE_PRIVATE_DEPS})
	foreach(dep IN LISTS MODULE_PUBLIC_DEPS MODULE_PRIVATE_DEPS)
		get_property(_dep_all GLOBAL PROPERTY "FLING_MODULE_${dep}_ALL_DEPS")
		list(APPEND _all_deps ${_dep_all})
	endforeach()
	list(REMOVE_DUPLICATES _all_deps)
	set_property(GLOBAL PROPERTY "FLING_MODULE_${NAME}_ALL_DEPS" "${_all_deps}")

	set(_module_dir "${CMAKE_CURRENT_SOURCE_DIR}")
	file(GLOB_RECURSE _module_sources CONFIGURE_DEPENDS
		"${_module_dir}/src/*.cpp"
		"${_module_dir}/src/*.h"
		"${_module_dir}/src/*.hpp"
		"${_module_dir}/src/*.inl"
		"${_module_dir}/inc/*.h"
		"${_module_dir}/inc/*.hpp"
		"${_module_dir}/inc/*.inl"
	)

	if(NOT _module_sources)
		message(FATAL_ERROR "fling_add_module(${NAME}): no sources under ${_module_dir}/inc or src")
	endif()

	if(MODULE_STATIC)
		add_library(${NAME} STATIC ${_module_sources})
	else()
		add_library(${NAME} SHARED ${_module_sources})
	endif()

	add_library(Fling::${NAME} ALIAS ${NAME})

	set(_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/Generated")
	file(MAKE_DIRECTORY "${_gen_dir}")
	configure_file(
		"${FLING_CMAKE_DIR}/FlingModuleAPI.h.in"
		"${_gen_dir}/${NAME}API.h"
		@ONLY
	)

	target_include_directories(${NAME}
		PUBLIC
			"${_module_dir}/inc"
			"${_gen_dir}"
		PRIVATE
			"${_module_dir}/src"
	)

	target_compile_definitions(${NAME} PRIVATE "FLING_${MODULE_UPPER}_BUILD=1")
	if(MODULE_STATIC)
		target_compile_definitions(${NAME} PUBLIC "FLING_${MODULE_UPPER}_STATIC=1")
	endif()

	set(_public_link)
	foreach(dep IN LISTS MODULE_PUBLIC_DEPS)
		if(TARGET "Fling::${dep}")
			list(APPEND _public_link "Fling::${dep}")
		else()
			list(APPEND _public_link "${dep}")
		endif()
	endforeach()

	set(_private_link)
	foreach(dep IN LISTS MODULE_PRIVATE_DEPS)
		if(TARGET "Fling::${dep}")
			list(APPEND _private_link "Fling::${dep}")
		else()
			list(APPEND _private_link "${dep}")
		endif()
	endforeach()

	target_link_libraries(${NAME}
		PUBLIC
			${_public_link}
			${MODULE_PUBLIC_LIBS}
		PRIVATE
			${_private_link}
			${MODULE_PRIVATE_LIBS}
	)

	set_target_properties(${NAME} PROPERTIES
		FOLDER "Fling/Modules"
		CXX_VISIBILITY_PRESET hidden
		VISIBILITY_INLINES_HIDDEN ON
	)

	if(MSVC)
		foreach(_source IN ITEMS ${_module_sources})
			get_filename_component(_source_path "${_source}" PATH)
			string(REPLACE "${CMAKE_SOURCE_DIR}" "" _group_path "${_source_path}")
			string(REPLACE "/" "\\" _group_path "${_group_path}")
			source_group("${_group_path}" FILES "${_source}")
		endforeach()
	endif()
endfunction()
