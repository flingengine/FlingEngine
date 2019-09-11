#pragma once

#include "Resource.h"
#include "stb_image.h"

namespace Fling
{
    /**
     * @brief   An image represents a 2D file that has data about each pixel in the image
     */
    class Image : public Resource
    {
    public:
        explicit Image(Guid t_ID, VkDevice t_Device, VkPhysicalDevice t_PhysDevice, void* t_Data = nullptr);
        virtual ~Image();

		INT32 GetWidth() const { return m_Width; }
		INT32 GetHeight() const { return m_Height; }
        INT32 GetChannels() const { return m_Channels; }
        stbi_uc* GetPixelData() const { return m_PixelData; }

        /**
         * @brief   Get the Image Size object (width * height * 4)
         *          Multiply by 4 because the pixel is laid out row by row with 4 bytes per pixel
         * @return INT32 
         */
        UINT64 GetImageSize() const { return m_Width * m_Height * 4; } 

    private:

		/**
		* @brief	Loads the Vulkan resources needed for this image
		*/
		void LoadVulkanImage(VkPhysicalDevice t_PhysDevice);

        /** Width of this image */
		INT32 m_Width = 0;

        /** Height of this image */
		INT32 m_Height = 0;

        /** The color channels of this image */
        INT32 m_Channels = 0;

        /** the actual pixel data that represents this image */
        stbi_uc* m_PixelData;

		/** The Vulkan image data */
		VkImage m_vVkImage;

		/** The Vulkan memory resource for this image */
		VkDeviceMemory m_VkMemory;

		/** The device that this image was created with */
		VkDevice m_Device;
    };
}   // namespace Fling