#include "pch.h"
#include "MultiSampler.h"
#include "Renderer.h"
#include "GraphicsHelpers.h"

namespace Fling
{
    Multisampler::Multisampler(VkExtent2D t_Extents, VkFormat t_Format, VkSampleCountFlagBits t_SampleCount /* = VK_SAMPLE_COUNT_1_BIT */)
        : m_SampleCountBits(t_SampleCount)
    {
        Create(t_Extents, t_Format);
    }

    void Multisampler::Create(VkExtent2D t_Extents, VkFormat t_Format)
    {
        VkFormat colorFormat = t_Format;

        //GraphicsHelpers::CreateVkImage(
        //    t_Extents.width,
        //    t_Extents.height, 
        //    /** Mip levels */ 1, 
        //    m_SampleCountBits, 
        //    colorFormat, 
        //    VK_IMAGE_TILING_OPTIMAL, 
        //    VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 
        //    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        //    m_ColorImage, 
        //    m_ColorImageMemory
        //);
        
        m_ColorImageView = GraphicsHelpers::CreateVkImageView(
            m_ColorImage, 
            colorFormat, 
            VK_IMAGE_ASPECT_COLOR_BIT, 
            1
        );

        GraphicsHelpers::TransitionImageLayout(
            m_ColorImage, 
            colorFormat, 
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