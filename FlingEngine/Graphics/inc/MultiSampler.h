#pragma once

#include "FlingVulkan.h"
#include "Platform.h"       // for FORCEINLINE

namespace Fling
{
    /**
     * @brief   A multi-sampler will allow us to enable MSAA. Should be recreated with the swap chain
     *          as it needs the most up to date extents
     */
    class Multisampler
    {
    public:

		/**
		* @brief	Creates a multi-sampler with the set sample count, but does not create it
		*/
		Multisampler(VkSampleCountFlagBits t_SampleCount = VK_SAMPLE_COUNT_1_BIT);

		/**
		* @brief	Initializes and creates this multi-sampler.
		* @param t_Extents		The extents of the current swap chain
		* @param t_Format		The same image format as your swap chain
		*/
        Multisampler(VkExtent2D t_Extents, VkFormat t_Format, VkSampleCountFlagBits t_SampleCount = VK_SAMPLE_COUNT_1_BIT);

        ~Multisampler();

        FORCEINLINE VkSampleCountFlagBits GetSampleCountFlagBits() const { return m_SampleCountBits; }
		FORCEINLINE const VkImageView& GetImageView() const { return m_ColorImageView; }

		/** Release the Image, Image memory, and Image view of this multi sampler.  */
        void Release();

		/** Create the image, image mem, and image view based on the m_SampleCountBits field. */
        void Create(VkExtent2D t_Extents, VkFormat t_Format);

    private:
        VkImage m_ColorImage = VK_NULL_HANDLE;
        VkDeviceMemory m_ColorImageMemory = VK_NULL_HANDLE;
        VkImageView m_ColorImageView = VK_NULL_HANDLE;

		/** The max sample count allowed on this device. Calculated in PhysicalDevice ctor */
        VkSampleCountFlagBits m_SampleCountBits = VK_SAMPLE_COUNT_1_BIT;
    }; 
}   // namespace Fling