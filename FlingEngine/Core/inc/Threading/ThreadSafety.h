#pragma once

/**
 * Clang -Wthread-safety attributes.
 *
 * The macros expand to nothing on GCC and MSVC. The std lock types have no
 * attributes, so the analysis only sees the Fling wrappers.
 */

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if defined(__clang__) && __has_attribute(capability)
#define FLING_THREAD_ANNOTATION(x) __attribute__((x))
#else
#define FLING_THREAD_ANNOTATION(x)
#endif

#define FLING_CAPABILITY(x) FLING_THREAD_ANNOTATION(capability(x))
#define FLING_SCOPED_CAPABILITY FLING_THREAD_ANNOTATION(scoped_lockable)

#define FLING_GUARDED_BY(x) FLING_THREAD_ANNOTATION(guarded_by(x))
#define FLING_PT_GUARDED_BY(x) FLING_THREAD_ANNOTATION(pt_guarded_by(x))

#define FLING_ACQUIRE(...) FLING_THREAD_ANNOTATION(acquire_capability(__VA_ARGS__))
#define FLING_ACQUIRE_SHARED(...) FLING_THREAD_ANNOTATION(acquire_shared_capability(__VA_ARGS__))

#define FLING_RELEASE(...) FLING_THREAD_ANNOTATION(release_capability(__VA_ARGS__))
#define FLING_RELEASE_SHARED(...) FLING_THREAD_ANNOTATION(release_shared_capability(__VA_ARGS__))
#define FLING_RELEASE_GENERIC(...) FLING_THREAD_ANNOTATION(release_generic_capability(__VA_ARGS__))

#define FLING_TRY_ACQUIRE(...) FLING_THREAD_ANNOTATION(try_acquire_capability(__VA_ARGS__))
#define FLING_TRY_ACQUIRE_SHARED(...) FLING_THREAD_ANNOTATION(try_acquire_shared_capability(__VA_ARGS__))

#define FLING_REQUIRES(...) FLING_THREAD_ANNOTATION(requires_capability(__VA_ARGS__))
#define FLING_REQUIRES_SHARED(...) FLING_THREAD_ANNOTATION(requires_shared_capability(__VA_ARGS__))

#define FLING_EXCLUDES(...) FLING_THREAD_ANNOTATION(locks_excluded(__VA_ARGS__))

#define FLING_ASSERT_CAPABILITY(x) FLING_THREAD_ANNOTATION(assert_capability(x))
#define FLING_RETURN_CAPABILITY(x) FLING_THREAD_ANNOTATION(lock_returned(x))

#define FLING_NO_THREAD_SAFETY_ANALYSIS FLING_THREAD_ANNOTATION(no_thread_safety_analysis)
