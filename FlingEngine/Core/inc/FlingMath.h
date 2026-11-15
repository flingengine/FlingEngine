#pragma once

// GLM
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL // So that we can hash our vertecies
// No GLM_FORCE_DEFAULT_ALIGNED_GENTYPES: GLM honours it only with MSVC language extensions (or SIMD on GCC and
// Clang), so it gave Windows 16-byte vec3s and Linux 12-byte ones. GLM types are packed on every platform; a struct
// that the GPU reads with std140 rules pads itself and checks its offsets (see Graphics' UBO structs).

#include "MathUtils.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

// The same layout on every platform and compiler: packed, 4-byte aligned GLM types. A build where these fail has
// another GLM configuration and would lay out components (and GPU buffers) differently from the others.
static_assert(GLM_CONFIG_ALIGNED_GENTYPES == GLM_DISABLE && GLM_CONFIG_SIMD == GLM_DISABLE,
              "GLM must use packed types without SIMD on every platform");
static_assert(sizeof(glm::vec2) == 8 && alignof(glm::vec2) == 4);
static_assert(sizeof(glm::vec3) == 12 && alignof(glm::vec3) == 4);
static_assert(sizeof(glm::vec4) == 16 && alignof(glm::vec4) == 4);
static_assert(sizeof(glm::quat) == 16 && alignof(glm::quat) == 4);
// The one layout difference left: GLM stores a quat as w, x, y, z when the compiler has Microsoft language extensions
// (MSVC, clang-cl) and as x, y, z, w otherwise, and no GLM option changes that. Use .x, .y, .z and .w, never the bytes
// of a quat, its operator[] or glm::value_ptr, for anything that must match across platforms (the state hash does).
static_assert(sizeof(glm::mat3) == 36 && alignof(glm::mat3) == 4);
static_assert(sizeof(glm::mat4) == 64 && alignof(glm::mat4) == 4);
