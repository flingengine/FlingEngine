#pragma once

/************************************************************************/
/* Common Headers                                                       */
/************************************************************************/

//#define FLING_API __declspec(dllexport)

#define F_ENABLE_LOGGING

#define F_INLINE    inline


#include <iostream>
#include <assert.h>
#include <functional>
#include <cstdlib>
#include <vector>
#include <unordered_map>
#include <map>
#include <cstring>
#include <set>
#include <array>

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

#define FLING_DEFAULT_WINDOW_WIDTH		800
#define FLING_DEFAULT_WINDOW_HEIGHT		600