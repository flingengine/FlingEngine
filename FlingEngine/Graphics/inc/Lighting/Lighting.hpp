#pragma once

#include "FlingTypes.h"
#include "Buffer.h"
#include "DirectionalLight.hpp"
#include "PointLight.hpp"
#include <vector>

namespace Fling
{
    struct Lighting
    {
        /** Dir Lights */
        static const uint32 MaxDirectionalLights = 32;
        uint32 m_CurrentDirLights = 0;

        /** Point Lights */
        static const uint32 MaxPointLights = 32;
        uint32 m_CurrentPointLights = 0;

        std::vector<Buffer*> m_LightingUBOs;
    };

}