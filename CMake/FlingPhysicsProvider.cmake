# Physics providers: modules that implement Fling::PhysicsProvider for one
# physics library. The engine finds them, and only the one that
# FLING_PHYSICS_PROVIDER selects is configured and compiled.
#
# A provider is a folder with a CMakeLists.txt that calls
#
#     fling_add_physics_provider( <Name> [PRIVATE_DEPS ...] [LIBS ...] )
#
# and a .cpp file that contains, at global scope,
#
#     FLING_IMPLEMENT_PHYSICS_PROVIDER( <Name>, <ProviderClass> )
#
# The module is named Physics<Name>. The engine does not need its header or its
# class name: the macro defines a factory function whose name comes from <Name>.
#
# The engine looks for providers in the folders of FLING_PHYSICS_PROVIDER_PATHS,
# one level deep. FlingEngine/ is always searched. A game adds its own folder
# before it adds the engine, with no engine change:
#
#     list( APPEND FLING_PHYSICS_PROVIDER_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/Source" )
#
# "None" is always available: the null provider in the Physics module.
#
# See Plans/physics.md, "How to add a provider".

# Create the module of a physics provider. Call it from the provider's
# CMakeLists.txt. The engine calls add_subdirectory on that folder only when
# the provider is selected.
#
#   NAME          The provider name, for example Jolt. The module is Physics<NAME>
#   PRIVATE_DEPS  Other Fling modules that the provider uses
#   LIBS          Libraries that the provider links PRIVATE (the physics library)
function(fling_add_physics_provider NAME)
	cmake_parse_arguments(PROVIDER "" "" "PRIVATE_DEPS;LIBS" ${ARGN})

	if(NOT NAME MATCHES "^[A-Za-z][A-Za-z0-9_]*$")
		message(FATAL_ERROR "fling_add_physics_provider(${NAME}): the name must be a C++ identifier")
	endif()

	set(_module "Physics${NAME}")
	fling_add_module(${_module}
		PUBLIC_DEPS
			Physics
		PRIVATE_DEPS
			${PROVIDER_PRIVATE_DEPS}
	)

	if(PROVIDER_LIBS)
		target_link_libraries(${_module} PRIVATE ${PROVIDER_LIBS})
	endif()

	# Physics results must not change with the compiler's float optimizations
	fling_set_deterministic_math(${_module})

	set_property(GLOBAL PROPERTY "FLING_PHYSICS_PROVIDER_MODULE_${NAME}" "${_module}")
endfunction()

# Find the providers in FLING_PHYSICS_PROVIDER_PATHS. This only reads files: it
# does not configure any provider. Sets FLING_PHYSICS_PROVIDER_NAMES, and
# FLING_PHYSICS_PROVIDER_DIR_<Name> for each provider, in the caller's scope.
function(fling_find_physics_providers)
	set(_names "None")

	foreach(_path IN LISTS FLING_PHYSICS_PROVIDER_PATHS)
		file(GLOB _candidates LIST_DIRECTORIES false "${_path}/CMakeLists.txt" "${_path}/*/CMakeLists.txt")
		foreach(_file IN LISTS _candidates)
			file(STRINGS "${_file}" _calls REGEX "^[ \t]*fling_add_physics_provider[ \t]*\\(")
			foreach(_call IN LISTS _calls)
				string(REGEX REPLACE "^[ \t]*fling_add_physics_provider[ \t]*\\([ \t]*([A-Za-z0-9_]+).*$" "\\1" _name "${_call}")
				get_filename_component(_dir "${_file}" DIRECTORY)

				if(_name STREQUAL "None")
					message(FATAL_ERROR "${_file}: the provider name None is reserved for the null provider")
				endif()
				if(DEFINED _dir_${_name} AND NOT _dir_${_name} STREQUAL _dir)
					message(FATAL_ERROR "Two physics providers are named ${_name}:\n  ${_dir_${_name}}\n  ${_dir}")
				endif()
				if(NOT DEFINED _dir_${_name})
					set(_dir_${_name} "${_dir}")
					list(APPEND _names ${_name})
					set(FLING_PHYSICS_PROVIDER_DIR_${_name} "${_dir}" PARENT_SCOPE)
				endif()
			endforeach()
		endforeach()
	endforeach()

	set(FLING_PHYSICS_PROVIDER_NAMES ${_names} PARENT_SCOPE)
endfunction()

# Add the provider that FLING_PHYSICS_PROVIDER selects, and make
# Fling::DefaultPhysicsProvider. Link that target to get the provider module
# and the generated DefaultPhysicsProvider.h, which has
# CreateDefaultPhysicsProvider() and DefaultPhysicsProviderName. Call it after
# the Physics module is added.
function(fling_add_selected_physics_provider)
	fling_find_physics_providers()
	set_property(CACHE FLING_PHYSICS_PROVIDER PROPERTY STRINGS ${FLING_PHYSICS_PROVIDER_NAMES})

	set(_name "${FLING_PHYSICS_PROVIDER}")
	if(NOT _name IN_LIST FLING_PHYSICS_PROVIDER_NAMES)
		list(JOIN FLING_PHYSICS_PROVIDER_NAMES ", " _known)
		message(FATAL_ERROR
			"FLING_PHYSICS_PROVIDER is '${_name}', but no provider has that name.\n"
			"Providers found: ${_known}\n"
			"Search paths (FLING_PHYSICS_PROVIDER_PATHS): ${FLING_PHYSICS_PROVIDER_PATHS}")
	endif()

	set(_module "")
	if(NOT _name STREQUAL "None")
		set(_dir "${FLING_PHYSICS_PROVIDER_DIR_${_name}}")
		add_subdirectory("${_dir}" "${CMAKE_BINARY_DIR}/PhysicsProviders/${_name}")

		get_property(_module GLOBAL PROPERTY "FLING_PHYSICS_PROVIDER_MODULE_${_name}")
		if(NOT _module OR NOT TARGET ${_module})
			message(FATAL_ERROR "${_dir}/CMakeLists.txt names the provider ${_name}, but did not call fling_add_physics_provider(${_name})")
		endif()
	endif()
	message(STATUS "Physics provider: ${_name}")

	set(FLING_PHYSICS_PROVIDER_NAME "${_name}")
	set(_gen_dir "${CMAKE_BINARY_DIR}/DefaultPhysicsProvider/Generated")
	configure_file("${FLING_ROOT_DIR}/CMake/DefaultPhysicsProvider.h.in" "${_gen_dir}/DefaultPhysicsProvider.h" @ONLY)

	add_library(FlingDefaultPhysicsProvider INTERFACE)
	add_library(Fling::DefaultPhysicsProvider ALIAS FlingDefaultPhysicsProvider)
	target_include_directories(FlingDefaultPhysicsProvider INTERFACE "${_gen_dir}")
	target_link_libraries(FlingDefaultPhysicsProvider INTERFACE Fling::Physics)
	if(_module)
		target_link_libraries(FlingDefaultPhysicsProvider INTERFACE Fling::${_module})
	endif()
endfunction()
