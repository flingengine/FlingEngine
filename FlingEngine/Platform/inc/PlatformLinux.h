#pragma once

#ifdef FLING_LINUX

#include <unistd.h>

// Break point, see https://stackoverflow.com/questions/17516855/portable-equivalent-of-debugbreak
#	define FLING_BREAK	asm("int $3")


#	define INLINE			inline
#	define FORCEINLINE		inline __attribute__ ((always_inline))

#include "FlingLinuxExports.h"
#include "DesktopWindow.h"

#include "Input/LinuxKeycodes.h"
#include "Input/LinuxInput.h"

#endif	// FLING_LINUX