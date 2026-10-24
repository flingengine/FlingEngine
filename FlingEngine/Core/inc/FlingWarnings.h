#pragma once

/**
 * Cross-platform macros for locally silencing a compiler warning. Use these
 * instead of writing compiler-specific #ifdef / #pragma blocks at call sites.
 *
 * Each F_*_DISABLE_WARNING macro only does something on its own compiler, so
 * a call site can list the warning for every compiler that needs it:
 *
 *     F_WARNING_PUSH
 *     F_MSVC_DISABLE_WARNING(4251)
 *     F_GCC_DISABLE_WARNING("-Wclass-memaccess")
 *     std::mt19937 m_Engine;
 *     F_WARNING_POP
 *
 * Always pair F_WARNING_PUSH with F_WARNING_POP and keep the region as small
 * as possible, with a comment saying why the warning is safe to ignore.
 */

#define F_PRAGMA_STRINGIFY(x) #x

// Clang is checked first because clang-cl also defines _MSC_VER
#if defined(__clang__)

#	define F_WARNING_PUSH                   _Pragma("clang diagnostic push")
#	define F_WARNING_POP                    _Pragma("clang diagnostic pop")
#	define F_CLANG_DISABLE_WARNING(Name)    _Pragma(F_PRAGMA_STRINGIFY(clang diagnostic ignored Name))
#	define F_GCC_DISABLE_WARNING(Name)
#	define F_MSVC_DISABLE_WARNING(Number)

#elif defined(_MSC_VER)

#	define F_WARNING_PUSH                   __pragma(warning(push))
#	define F_WARNING_POP                    __pragma(warning(pop))
#	define F_CLANG_DISABLE_WARNING(Name)
#	define F_GCC_DISABLE_WARNING(Name)
#	define F_MSVC_DISABLE_WARNING(Number)   __pragma(warning(disable : Number))

#elif defined(__GNUC__)

#	define F_WARNING_PUSH                   _Pragma("GCC diagnostic push")
#	define F_WARNING_POP                    _Pragma("GCC diagnostic pop")
#	define F_CLANG_DISABLE_WARNING(Name)
#	define F_GCC_DISABLE_WARNING(Name)      _Pragma(F_PRAGMA_STRINGIFY(GCC diagnostic ignored Name))
#	define F_MSVC_DISABLE_WARNING(Number)

#else

#	define F_WARNING_PUSH
#	define F_WARNING_POP
#	define F_CLANG_DISABLE_WARNING(Name)
#	define F_GCC_DISABLE_WARNING(Name)
#	define F_MSVC_DISABLE_WARNING(Number)

#endif
