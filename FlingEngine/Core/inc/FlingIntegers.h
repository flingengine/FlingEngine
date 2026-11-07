#pragma once

#include <cstdint>

// The integer types of the engine. Use these, not std::uint32_t or uint32_t.
// This header has no other includes, so light headers can use it.
typedef uint8_t                 uint8;
typedef unsigned short          uint16;
typedef uint32_t                uint32;
typedef uint64_t                uint64;

typedef int8_t                  int8;
typedef signed short            int16;
typedef int32_t                 int32;
typedef int64_t                 int64;
