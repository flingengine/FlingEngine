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
        explicit Image(Guid t_ID, void* t_Data = nullptr);
        virtual ~Image();

		FORCEINLINE UINT32 GetWidth() const { return m_Width; }
		FORCEINLINE UINT32 GetHeight() const { return m_Height; }
		FORCEINLINE INT32 GetChannels() const { return m_Channels; }
        
		FORCEINLINE const VkImage& GetVkImage() const { return m_vVkImage; }
		FORCEINLINE const VkImageView& GetVkImageView() const { return m_ImageView; }
		FORCEINLINE const VkSampler& GetSampler() const { return m_TextureSampler; }

        /**
         * @brief   Get the Image Size object (width * height * 4)
         *          Multiply by 4 because the pixel is laid out row by row with 4 bytes per pixel
         * @return INT32 
         */
        UINT64 GetImageSize() const { return m_Width * m_Height * 4; } 

		/**
		* @brief	Release the Vulkan resources of this image 
		*/
		void Release();

    private:

		/**
		* @brief	Loads the Vulkan resources needed for this image
		*/
		void LoadVulkanImage();

        /**
         * @brief Create a Image View object that is needed to sample this image from the swap chain
         */
        void CreateImageView();

		void CreateTextureSampler();

		void CopyBufferToImage(VkBuffer t_Buffer);

        /** Width of this image */
		UINT32 m_Width = 0;

        /** Height of this image */
		UINT32 m_Height = 0;

        /** The color channels of this image */
        INT32 m_Channels = 0;

		/** The Vulkan image data */
		VkImage m_vVkImage;

        /** The view of this image for the swap chain */
        VkImageView m_ImageView;

		VkSampler m_TextureSampler;

		/** The Vulkan memory resource for this image */
		VkDeviceMemory m_VkMemory;
    };
}   // namespace Fling