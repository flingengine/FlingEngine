#pragma once

#if FLING_LINUX

#   if __GNUC__ >= 4
#       define FLING_API            __attribute__ ((visibility ("default")))
#       define FLING_NOT_EXPORTED   __attribute__ ((visibility ("hidden")))
#       define FLING_EXTERN         
#   else
#       define FLING_API
#       define FLING_NOT_EXPORTED
#       define FLING_EXTERN         extern
#   endif

#endif	// FLING_LINUX