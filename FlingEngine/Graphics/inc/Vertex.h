#pragma once

#include "FlingVulkan.h"

namespace Fling
{
    /**
    * Basic Vertex outline for use with our vertex buffers
    */
    struct Vertex
    {
        glm::vec3 Pos {};
        glm::vec3 Color {};
        glm::vec3 Tangent {};
        glm::vec3 Normal {};
		glm::vec2 TexCoord {};

		bool operator==(const Vertex& other) const 
		{
			return Pos == other.Pos && Color == other.Color && TexCoord == other.TexCoord && Tangent == other.Tangent;
		}

		/**
		 * @brief	Gets the shader binding of a vertex
		 */
        static VkVertexInputBindingDescription GetBindingDescription() 
        {
            VkVertexInputBindingDescription bindingDescription = {};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

		//#TODO Use shader reflection to get our bindings for this vertex instead
        static std::array<VkVertexInputAttributeDescription, 5> GetAttributeDescriptions()
        {
            std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions = {};

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, Pos);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, Color);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(Vertex, Tangent);

			attributeDescriptions[3].binding = 0;
			attributeDescriptions[3].location = 3;
			attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[3].offset = offsetof(Vertex, TexCoord);

            attributeDescriptions[4].binding = 0;
            attributeDescriptions[4].location = 4;
            attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[4].offset = offsetof(Vertex, Normal);

            return attributeDescriptions;
        }

    };
}   // namespace Fling

// Hash function for a vertex so that we can put thing std::maps and what not
// @see https://vulkan-tutorial.com/Loading_models
namespace std
{
	template<> struct hash<Fling::Vertex>
	{
		size_t operator()(Fling::Vertex const& vertex) const
		{
			return	((hash<glm::vec3>()(vertex.Pos) ^
					(hash<glm::vec3>()(vertex.Color) << 1)) >> 1) ^
					(hash<glm::vec2>()(vertex.TexCoord) << 1);
		}
	};
}