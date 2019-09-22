#pragma once

// GLM 
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES  // Force GLM to use a version of vec2 and mat4 that have specific alignments
#define GLM_ENABLE_EXPERIMENTAL	// So that we can hash our vertecies 

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>