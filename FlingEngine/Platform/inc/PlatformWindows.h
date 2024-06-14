#pragma once

#if FLING_WINDOWS

#if defined(_DEBUG)

#   define FLING_DEBUG 1
//#	define _CRTDBG_MAP_ALLOC  

#include <stdlib.h>
#include <crtdbg.h> 

#ifdef _MSC_VER
#	define FLING_BREAK(...)	__debugbreak()
#else
#	define FLING_BREAK(...)	asm("int $3")
#endif	// _MSC_VER

#else   // Not debug

#   define FLING_DEBUG 0
#	define FLING_BREAK(...) 

#endif	// _DEBUG


#   define WIN32_LEAN_AND_MEAN

// Windows.h will define it's own versions of min/max, which will make 
// entt fail to compile with a bunch of templated std::min errors like: 
//	
//	error C2589: '(' : illegal token on right side of '::'
//	error C2059 : syntax error : '::'
// 
// This just absolutely ensures that does not happen :) 
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <direct.h>

#   ifdef _MSC_VER

#	pragma warning(disable:26495)
#	pragma warning(disable:4267)
#	pragma warning(disable:26451)
#	pragma warning(disable:4996)

#   endif   // _MSC_VER

#	define INLINE			        inline
#	define FORCEINLINE		        __forceinline

#include "FlingWindowsExports.h"

#include "DesktopWindow.h"

#include "Input/WindowsKeycodes.h"
#include "Input/WindowsInput.h"

#endif	// FLING_WINDOWS