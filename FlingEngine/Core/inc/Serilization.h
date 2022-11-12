#pragma once

#include "Platform.h"

// Disable the cereal warnings
#if FLING_LINUX

// Clang -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#if defined(__clang__)

//#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wexceptions"
#pragma clang diagnostic ignored "-Wunused-private-field"

// GCC -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#elif defined(__GNUC__)

//#pragma GCC diagnostic ignored "-Wunused-private-field"
//#pragma GCC diagnostic ignored "-Wexceptions"

#endif


#elif FLING_WINDOWS

// TODO: MSVC 

#endif

#include <cereal/archives/json.hpp>


// Enable the warnings again
#if FLING_LINUX

// Clang -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#if defined(__clang__)

//#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wexceptions"
#pragma clang diagnostic ignored "-Wunused-private-field"

// GCC -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#elif defined(__GNUC__)

//#pragma GCC diagnostic ignored "-Wunused-private-field"
//#pragma GCC diagnostic ignored "-Wexceptions"

#endif

//#pragma GCC diagnostic ignored "-Wunused-private-field"
//#pragma GCC diagnostic ignored "-Wexceptions"

#elif FLING_WINDOWS

// TODO: MSVC 

#endif