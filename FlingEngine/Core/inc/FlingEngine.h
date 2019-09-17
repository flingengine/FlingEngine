#pragma once


#include "pch.h"
#include "Engine.h"
#include "Renderer.h"
#include "Platform.h"
#include "Logger.h"
#include "Timing.h"
#include "ResourceManager.h"
#include "FlingConfig.h"
#include "NonCopyable.hpp"
#include "World.h"
#include <nlohmann/json.hpp>
#include "FirstPersonCamera.h"
#include "FlingWindow.h"
#include "Vertex.h"
#include "Singleton.hpp"

#ifndef GLFW_INCLUDE_VULKAN
#	define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

#include "UniformBufferObject.h"