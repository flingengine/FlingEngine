#pragma once

// The engine allocators take pages from the OS, so AddressSanitizer does not know
// which bytes of them are in use. These macros tell it: memory that an allocator has
// not handed out is "poisoned", and a read or write of it is reported like any other
// use after free. In a build with no AddressSanitizer they do nothing.

#if defined(__SANITIZE_ADDRESS__)
#define FLING_ADDRESS_SANITIZER 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define FLING_ADDRESS_SANITIZER 1
#endif
#endif
#ifndef FLING_ADDRESS_SANITIZER
#define FLING_ADDRESS_SANITIZER 0
#endif

#if FLING_ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h>
/** Mark bytes as not in use. AddressSanitizer reports a read or write of them. */
#define FLING_POISON_MEMORY(address, bytes) ASAN_POISON_MEMORY_REGION((address), (bytes))
/** Mark bytes as in use again. */
#define FLING_UNPOISON_MEMORY(address, bytes) ASAN_UNPOISON_MEMORY_REGION((address), (bytes))
#else
#define FLING_POISON_MEMORY(address, bytes) ((void)(address), (void)(bytes))
#define FLING_UNPOISON_MEMORY(address, bytes) ((void)(address), (void)(bytes))
#endif
