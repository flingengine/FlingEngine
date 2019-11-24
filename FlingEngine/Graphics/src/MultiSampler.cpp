#include "pch.h"
#include "MultiSampler.h"
#include "Renderer.h"
#include "GraphicsHelpers.h"
#include "LogicalDevice.h"

namespace Fling
{
	Multisampler::Multisampler(LogicalDevice* t_Dev, VkSampleCountFlagBits t_SampleCount)
		: m_SampleCountBits(t_SampleCount)
		, m_Device(t_Dev)
	{
	}

	Multisampler::Multisampler(VkExtent2D t_Extents, VkFormat t_Format, VkSampleCountFlagBits t_SampleCount /* = VK_SAMPLE_COUNT_1_BIT */)
        : m_SampleCountBits(t_SampleCount)
    {
        Create(t_Extents, t_Format);
    }

    void Multisampler::Create(VkExtent2D t_Extents, VkFormat t_Format)
    {
		// t_Format should the same format as the swap chain
		GraphicsHelpers::CreateVkImage(
			m_Device->GetVkDevice(),
			t_Extents.width,
			t_Extents.height,
			/* Mip levels */ 1,
			/* Depth */ 1,
			/* Array Layers */ 1,
			/* Format */ t_Format,
			/* Tiling */ VK_IMAGE_TILING_OPTIMAL,
			/* Usage */ VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			/* Props */ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			/* Flags */ 0,
			m_ColorImage,
			m_ColorImageMemory,
			m_SampleCountBits
		);

		m_ColorImageView = GraphicsHelpers::CreateVkImageView(
			m_ColorImage,
			t_Format,
			VK_IMAGE_ASPECT_COLOR_BIT,
			1
		);

		GraphicsHelpers::TransitionImageLayout(
			m_ColorImage,
			t_Format,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			/* mip levels */ 1
		);
    }

    void Multisampler::Release()
    {
        VkDevice device = Renderer::Get().GetLogicalVkDevice();
        if(m_ColorImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(device, m_ColorImage, nullptr);
            m_ColorImage = VK_NULL_HANDLE;
        }

        if(m_ColorImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, m_ColorImageView, nullptr);
            m_ColorImageView = VK_NULL_HANDLE;
        }

        if(m_ColorImageMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, m_ColorImageMemory, nullptr);
            m_ColorImageMemory = VK_NULL_HANDLE;
        }
    }

    Multisampler::~Multisampler()
    {
        Release();
    }

}   // namespace Fling