#pragma once

#if defined FLING_WINDOWS

#include <Windows.h>
#include <direct.h>

#	define INLINE			inline
#	define FORCEINLINE		__forceinline

#	ifdef FLING_EXPORT		// Export functions
#		ifdef __GNUC__
#			define FLING_API __attribute__ ((dllexport))
#		else
#			define FLING_API __declspec(dllexport) 
#		endif
#	else					// Not exporting function
#	ifdef __GNUC__
#		define FLING_API __attribute__ ((dllimport))
#	else
#		define FLING_API __declspec(dllimport) 
#	endif
#	endif
#	define FLING_NOT_EXPORTED

#endif	// FLING_WINDOWS