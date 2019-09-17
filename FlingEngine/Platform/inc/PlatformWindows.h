#pragma once

#if defined FLING_WINDOWS

#if defined(_DEBUG)

#	define _CRTDBG_MAP_ALLOC  

#include <stdlib.h>
#include <crtdbg.h> 

#	define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#	define new DEBUG_NEW

#endif	// _DEBUG


#   define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <direct.h>

#   ifdef _MSC_VER

#	pragma warning(disable:26495)
#	pragma warning(disable:4267)
#	pragma warning(disable:26451)

#   endif   // _MSC_VER

#	define INLINE			        inline
#	define FORCEINLINE		        __forceinline

// API Export/Import

#	define FLING_NOT_EXPORTED

#	ifdef FLING_EXPORT		        // Export functions
#		define FLING_API        __declspec(dllexport) 
#       define FLING_EXTERN      
#	else					        // Not exporting function
#		define FLING_API        
#       define FLING_EXTERN        extern
#	endif

#include "DesktopWindow.h"

#include "Input/WindowsKeycodes.h"
#include "Input/WindowsInput.h"

#endif	// FLING_WINDOWS