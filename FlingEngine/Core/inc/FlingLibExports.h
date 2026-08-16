#pragma once

/**
 * Primitive import/export attributes used by generated per-module *API.h headers.
 * Do not use these directly on engine types — use CORE_API, GRAPHICS_API, etc.
 */

#if defined(_WIN32)
#	define FLING_LIB_EXPORT __declspec(dllexport)
#	define FLING_LIB_IMPORT __declspec(dllimport)
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#	define FLING_LIB_EXPORT __attribute__((visibility("default")))
#	define FLING_LIB_IMPORT
#else
#	define FLING_LIB_EXPORT
#	define FLING_LIB_IMPORT
#endif
