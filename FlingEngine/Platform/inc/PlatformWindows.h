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

#include "FlingWindowsExports.h"

#include "DesktopWindow.h"

#include "Input/WindowsKeycodes.h"
#include "Input/WindowsInput.h"

#endif	// FLING_WINDOWS