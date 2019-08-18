#pragma once

#include "pch.h"

namespace Fling
{
    /**
    * UniformBufferObjectRepresents the UBO in C++
    */
    struct UniformBufferObject
    {
        glm::mat4 Model;
        glm::mat4 View;
        glm::mat4 Proj;
    };
}   // namespace Fling