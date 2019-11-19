#pragma once 

#include "Resource.h"
#include "stb_image.h"

namespace Fling
{
    class Image : public Resource
    {
    public:
        static std::shared_ptr<Fling::Image> Create(Guid t_ID, void* t_Data = nullptr);

        explicit Image(Guid t_ID, void* t_Data = nullptr);
        virtual ~Image;

        FORCEINLINE UINT32 GetWidth() const { return m_Width; }
		FORCEINLINE UINT32 GetHeight() const { return m_Height; }
		FORCEINLINE INT32 GetChannels() const { return m_Channels; }
        FORCEINLINE UINT32 GetMipLevels() const { return m_MipLevels; }

        FORCEINLINE const VkImage& GetVkImage() const { return m_vVkImage; }
		FORCEINLINE const VkImageView& GetVkImageView() const { return m_ImageView; }
		FORCEINLINE const VkSampler& GetSampler() const { return m_TextureSampler; }
		FORCEINLINE VkDescriptorImageInfo* GetDescriptorInfo() { return &m_ImageInfo; }
        FORCEINLINE const vkFormat& GetVkImageFormat() const { return m_Format; }
   
        /**
         * @brief Get the Image Size object 
         *        Multiply by 2 * 3 because there are 3 channels that are 2 bytes each
         *        Each channel is represented as a signed 16 (bit) float 
         * @return UINT64 
         */
        UINT64 GetImageSize() const { return m_Width * m_Height * 6; }



    private:
        UINT32 m_Width = 0;

        UINT32 m_Height = 0;

        UINT32 m_MipLevels = 0;

        INT32 m_Channels = 0;

        VkImage m_Image;

        VkImageView m_ImageView;

        VkSampler m_TextureSampler;

        VkDeviceMemory m_Memory;

        vkDescriptorImageInfo m_ImageInfo = {};

        float* m_PixelData;

        vkFormat m_Format = VK_FORMAT_R16G16B16_SFLOAT;
    }
}