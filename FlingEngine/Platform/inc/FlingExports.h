#pragma once

//////////////////////////////////////////////////////////////////////////
// General platform abstraction 

#ifdef _WIN32		

#define FLING_APPLE			0
#define FLING_WINDOWS		1	
#define FLING_LINUX			0

#include "FlingWindowsExports.h"

#elif __APPLE__							

#define FLING_APPLE			1
#define FLING_WINDOWS		0	
#define FLING_LINUX			0

#error Fling does not support Apple... does it?

#elif __linux__							

#define FLING_APPLE			0
#define FLING_WINDOWS		0	
#define FLING_LINUX			1

#include "FlingLinuxExports.h"

#else									

#   error "Unknown compiler"

#endif