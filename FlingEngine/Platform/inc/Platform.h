#pragma once

//////////////////////////////////////////////////////////////////////////
// General platform abstraction 

#ifdef _WIN32		

#define FLING_WINDOWS
#include "PlatformWindows.h"

#elif __APPLE__							

#define FLING_APPLE

#error Fling does not support Apple... does it?

#elif __linux__							

#define FLING_LINUX
#include "PlatformLinux.h"

#else									

#   error "Unknown compiler"

#endif


// #TODO: Any other platform (i.e. consoles, android, etc)