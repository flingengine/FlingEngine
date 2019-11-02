#pragma once

#include "Platform.h"

// Disable the cereal warnings
#if FLING_LINUX

#pragma GCC diagnostic ignored "-Wunused-private-field"
#pragma GCC diagnostic ignored "-Wexceptions"

#elif FLING_WINDOWS

// TODO: MSVC 

#endif

#include <cereal/archives/json.hpp>


// Enable the warnings again
#if FLING_LINUX

#pragma GCC diagnostic ignored "-Wunused-private-field"
#pragma GCC diagnostic ignored "-Wexceptions"

#elif FLING_WINDOWS

// TODO: MSVC 

#endif