#pragma once

/************************************************************************/
/* Common Headers                                                       */
/************************************************************************/
#define FLING_API
#define F_ENABLE_LOGGING

#include <iostream>
#include <assert.h>
#include <functional>
#include <cstdlib>
#include <vector>
#include <unordered_map>

#include "FlingTypes.h" 
#include "Random.h"
#include "Logger.h"

// GLFW
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// GLM 
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>