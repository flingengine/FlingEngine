#pragma once

#include "Resource.h"
#include "stb_image.h"

namespace Fling
{
    /**
     * @brief   An image represents a 2D file that has data about each pixel in the image
     */
    class Texture : public Resource
    {
    public:

		static std::shared_ptr<Fling::Texture> Create(Guid t_ID);

        explicit Texture(Guid t_ID);
        virtual ~Texture();

		FORCEINLINE uint32 GetWidth() const { return m_Width; }
		FORCEINLINE uint32 GetHeight() const { return m_Height; }
		FORCEINLINE int32 GetChannels() const { return m_Channels; }
        FORCEINLINE uint32 GetMipLevels() const { return m_MipLevels; }

		FORCEINLINE const VkImage& GetVkImage() const { return m_vVkImage; }
		FORCEINLINE const VkImageView& GetVkImageView() const { return m_ImageView; }
		FORCEINLINE const VkSampler& GetSampler() const { return m_TextureSampler; }
		FORCEINLINE VkDescriptorImageInfo* GetDescriptorInfo() { return &m_ImageInfo; }
        FORCEINLINE const VkFormat& GetVkImageFormat() const { return m_Format; }
        /**
         * @brief   Get the Image Size object (width * height * 4)
         *          Multiply by 4 because the pixel is laid out row by row with 4 bytes per pixel
         * @return int32 
         */
        uint64 GetImageSize() const { return m_Width * m_Height * 4; } 

        /**
         * @brief Get the Pixel Data object
         * 
         * @return stbi_uc* 
         */
        stbi_uc* GetPixelData() const { return m_PixelData; }

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

        void GenerateMipMaps(VkFormat imageFormat);

        /** Width of this image */
		uint32 m_Width = 0;

        /** Height of this image */
		uint32 m_Height = 0;

        uint32 m_MipLevels = 0;

        /** The color channels of this image */
        int32 m_Channels = 0;

		/** The Vulkan image data */
		VkImage m_vVkImage;

        /** The view of this image for the swap chain */
        VkImageView m_ImageView;

		VkSampler m_TextureSampler;

		/** The Vulkan memory resource for this image */
		VkDeviceMemory m_VkMemory;

		VkDescriptorImageInfo m_ImageInfo{};
        
        /** Pixel data of image **/
        stbi_uc* m_PixelData;

        VkFormat m_Format = VK_FORMAT_R8G8B8A8_UNORM;
    };
}   // namespace Fling