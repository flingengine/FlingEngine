# Core module: platform macros, logging, math, memory, timing, input bindings,
# and engine paths. Every other module depends on Core, so Core must not
# include or link any other engine module.

fling_add_module(Core
	PUBLIC_LIBS
		Threads::Threads
		FlingExt::glm
		FlingExt::spdlog
		FlingExt::entt
		FlingExt::cereal
)

# Feature flags are read from Core headers (FlingTypes.h) and from every module
# that links Core, so they live here and propagate to everything.
target_compile_definitions(Core PUBLIC
	WITH_IMGUI=$<BOOL:${WITH_IMGUI_FLAG}>
	WITH_EDITOR=$<BOOL:${WITH_EDITOR_FLAG}>
	WITH_CRASH_HANDLER=$<BOOL:${WITH_CRASH_HANDLER_FLAG}>
	$<$<BOOL:${DEFINE_SHIPPING}>:FLING_SHIPPING>
)

#### Generated files (into the module's PUBLIC Generated/ dir) ####
set(_core_gen "${CMAKE_CURRENT_BINARY_DIR}/Generated")

execute_process(
	COMMAND git rev-parse --abbrev-ref HEAD
	WORKING_DIRECTORY ${FLING_ROOT_DIR}
	OUTPUT_VARIABLE GIT_BRANCH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
	COMMAND git log -1 --format=%h
	WORKING_DIRECTORY ${FLING_ROOT_DIR}
	OUTPUT_VARIABLE GIT_COMMIT_HASH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
configure_file(inc/GitVersion_TEMPLATE.h.in "${_core_gen}/GitVersion.h")

# Absolute asset/config/log paths for non-shipping builds
set(FLING_ENGINE_SOURCE_PATH "${FLING_ROOT_DIR}/FlingEngine")
configure_file(src/FlingAssetPaths.cpp.in "${_core_gen}/FlingAssetPaths.cpp")
target_sources(Core PRIVATE "${_core_gen}/FlingAssetPaths.cpp")

# CrashHandler reads symbol names for stack traces with DbgHelp
if(WIN32 AND WITH_CRASH_HANDLER_FLAG)
	target_link_libraries(Core PRIVATE dbghelp)
endif()
