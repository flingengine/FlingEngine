#pragma once

#ifdef FLING_LINUX

#include <unistd.h>


#	define INLINE			inline
#	define FORCEINLINE		inline __attribute__ ((always_inline))

#   if __GNUC__ >= 4
#       define FLING_API            __attribute__ ((visibility ("default")))
#       define FLING_NOT_EXPORTED   __attribute__ ((visibility ("hidden")))
#       define FLING_EXTERN         
#   else
#       define FLING_API
#       define FLING_NOT_EXPORTED
#       define FLING_EXTERN         extern
#   endif

#include "DesktopWindow.h"

#include "LinuxKeycodes.h"
#include "LinuxInput.h"

#endif	// FLING_LINUX