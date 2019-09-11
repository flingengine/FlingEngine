#pragma once

#ifdef FLING_LINUX

#include <unistd.h>


#	define INLINE			inline
#	define FORCEINLINE		inline __attribute__ ((always_inline))

#include "FlingLinuxExports.h"
#include "DesktopWindow.h"

#include "Input/LinuxKeycodes.h"
#include "Input/LinuxInput.h"

#endif	// FLING_LINUX