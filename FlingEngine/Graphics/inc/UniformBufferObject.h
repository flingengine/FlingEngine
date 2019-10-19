#pragma once

#include "pch.h"

#include "Buffer.h"

namespace Fling
{
	/** Representation of the dynamic Uniform buffer obj */
	struct UboDataDynamic
	{
		// Due to alignment we have to use a pointer to a mat4
		glm::mat4* Model = nullptr;
		Buffer* View = nullptr;
		Buffer* Dynamic = nullptr;

		void Release();

		~UboDataDynamic();
	};

	/** Representation of what vertex data of the dynamic UBO that there is only one of  */
	struct UboVS
	{
		glm::mat4 Projection;
		glm::mat4 View;
	};

    struct UboSkyboxVS
    {
        glm::mat4 Projection;
        glm::mat4 ModelView;
    };
}   // namespace Fling