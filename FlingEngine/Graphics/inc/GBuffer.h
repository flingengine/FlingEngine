#pragma  once

#include "FlingVulkan.h"

namespace Fling
{
	struct GBuffer
	{
		/** Image format of the Albedo sampler */
		VkFormat AlbedoFormat { VK_FORMAT_R8G8B8A8_UNORM };
		// Image format of the normal Samplers
		VkFormat NormalFormat { VK_FORMAT_A2B10G10R10_UNORM_PACK32 };
		VkImageUsageFlags rt_usage_flags { VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT };

		// Depth sampler
		// Normal sampler
		// Albedo sampler
	};
}   // namespace Fling