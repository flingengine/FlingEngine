#pragma once

#include "FlingTypes.h"
#include "Buffer.h"
#include "DirectionalLight.hpp"
#include "PointLight.hpp"

namespace Fling
{
    struct Lighting
    {
        /** Dir Lights */
        static const UINT32 MaxDirectionalLights = 32;
        UINT32 m_CurrentDirLights = 0;

        /** Point Lights */
        static const UINT32 MaxPointLights = 32;
        UINT32 m_CurrentPointLights = 0;

        std::vector<Buffer*> m_LightingUBOs;
    };

}