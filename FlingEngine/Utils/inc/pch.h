#pragma once

/************************************************************************/
/* STD library Functionality                                            */
/************************************************************************/
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

/************************************************************************/
/* Fling Engine Functionality                                           */
/************************************************************************/

// Enable Fling Logging (even applies in release)
#define F_ENABLE_LOGGING

// If defined then Fling will export functions to the DLL/Shared lib
#define FLING_EXPORT

#include "Platform.h"
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