# Header-only third-party dependencies, exposed as INTERFACE targets.
#
# Link these (e.g. FlingExt::glm) instead of adding external/ folders with
# include_directories(), so only targets that ask for a library see its headers.
# Libraries with their own CMake targets (glfw, spirv-cross-*, ImGui) are linked
# directly by name.

function(fling_add_external NAME)
	cmake_parse_arguments(EXT "" "" "INCLUDE_DIRS;DEFINITIONS" ${ARGN})

	add_library(fling_ext_${NAME} INTERFACE)
	add_library(FlingExt::${NAME} ALIAS fling_ext_${NAME})

	# SYSTEM so warnings inside vendored headers (e.g. Clang's
	# -Wdeprecated-literal-operator in EnTT and nlohmann/json) don't show up in
	# our builds. Warnings in first-party code are unaffected.
	target_include_directories(fling_ext_${NAME} SYSTEM INTERFACE ${EXT_INCLUDE_DIRS})
	if(EXT_DEFINITIONS)
		target_compile_definitions(fling_ext_${NAME} INTERFACE ${EXT_DEFINITIONS})
	endif()
endfunction()

set(_ext "${FLING_ROOT_DIR}/external")

# GLM_FORCE_CXX17 is needed for Clang support of glm::hash
fling_add_external(glm           INCLUDE_DIRS "${_ext}/glm" DEFINITIONS GLM_FORCE_CXX17)
fling_add_external(spdlog        INCLUDE_DIRS "${_ext}/spdlog/include")
fling_add_external(inih          INCLUDE_DIRS "${_ext}/inih")
fling_add_external(entt          INCLUDE_DIRS "${_ext}/entt/src")
fling_add_external(json          INCLUDE_DIRS "${_ext}/json/single_include")
fling_add_external(stb           INCLUDE_DIRS "${_ext}/stb")
fling_add_external(tinyobjloader INCLUDE_DIRS "${_ext}/tinyobjloader")
fling_add_external(cereal        INCLUDE_DIRS "${_ext}/cereal/include")

unset(_ext)
