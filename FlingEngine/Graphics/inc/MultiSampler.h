#pragma once

#include "FlingVulkan.h"
#include "Platform.h"       // for FORCEINLINE

namespace Fling
{
    /**
     * @brief   A multismapler will allow us to enable MSAA. Should be recreated with the swap chain
     *          as it needs the most up to date extents
     */
    class Multisampler
    {
    public:

        Multisampler(VkExtent2D t_Extents, VkFormat t_Format, VkSampleCountFlagBits t_SampleCount = VK_SAMPLE_COUNT_1_BIT);

        ~Multisampler();

        FORCEINLINE const VkSampleCountFlagBits& GetSampleCountFlagBits() const { return m_SampleCountBits; }

        void Release();

        void Create(VkExtent2D t_Extents, VkFormat t_Format);

    private:
        VkImage m_ColorImage = VK_NULL_HANDLE;
        VkDeviceMemory m_ColorImageMemory = VK_NULL_HANDLE;
        VkImageView m_ColorImageView = VK_NULL_HANDLE;

        VkSampleCountFlagBits m_SampleCountBits = VK_SAMPLE_COUNT_1_BIT;
    }; 
}   // namespace Fling