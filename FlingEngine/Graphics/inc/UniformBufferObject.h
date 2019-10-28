#pragma once

#include "pch.h"

#include "Buffer.h"

namespace Fling
{
	/** Representation of what vertex data of the dynamic UBO that there is only one of  */
	struct UboVS
	{
		glm::mat4 Model;
		glm::mat4 Projection;
		glm::mat4 View;
		glm::vec4 CamPos;
		// The world position of this object
		glm::vec3 ObjPos;
	};

    struct UboSkyboxVS
    {
        glm::mat4 Projection;
        glm::mat4 ModelView;
    };
}   // namespace Fling