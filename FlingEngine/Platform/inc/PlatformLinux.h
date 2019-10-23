#pragma once

#if FLING_LINUX

#ifndef NDEBUG
#   define FLING_DEBUG 1
#else
#   define FLING_DEBUG 0
#endif

#include <unistd.h>

// Break point, see https://stackoverflow.com/questions/17516855/portable-equivalent-of-debugbreak
#	define FLING_BREAK(str)	{ asm("int $3"); }


#	define INLINE			inline
#	define FORCEINLINE		inline __attribute__ ((always_inline))

#include "FlingLinuxExports.h"
#include "DesktopWindow.h"

#include "Input/LinuxKeycodes.h"
#include "Input/LinuxInput.h"

#endif	// FLING_LINUX