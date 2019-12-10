#pragma once 

#include "Resource.h"
#include "stb_image.h"

namespace Fling
{
	class LogicalDevice;
    /**
     * @brief Loads image R16G16B16_SFLOAT file formats
     *  exmplae file format : .hdr
     */
    class HDRImage : public Resource
    {
    public:
        static std::shared_ptr<Fling::HDRImage> Create(Guid t_ID, LogicalDevice* t_dev, void* t_Data = nullptr);

        explicit HDRImage(Guid t_ID, LogicalDevice* t_dev, void* t_Data = nullptr);
        virtual ~HDRImage();

        FORCEINLINE UINT32 GetWidth() const { return m_Width; }
        FORCEINLINE UINT32 GetHeight() const { return m_Height; }
        FORCEINLINE INT32 GetChannels() const { return m_Channels; }
        FORCEINLINE UINT32 GetMipLevels() const { return m_MipLevels; }

        FORCEINLINE const VkImage& GetVkImage() const { return m_Image; }
        FORCEINLINE const VkImageView& GetVkImageView() const { return m_ImageView; }
        FORCEINLINE const VkSampler& GetSampler() const { return m_TextureSampler; }
        FORCEINLINE VkDescriptorImageInfo* GetDescriptorInfo() { return &m_ImageInfo; }
        FORCEINLINE const VkFormat& GetVkImageFormat() const { return m_Format; }

        /**
         * @brief Get the Image Size object
         *        Multiply by 2 * 3 because there are 3 channels that are 2 bytes each
         *        Each channel is represented as a signed 16 (bit) float
         * @return UINT64
         */
        UINT64 GetImageSize() const { return m_Width * m_Height * 6; }
        /**
         * @brief Get the Pixel Data as signed floats
         * 
         * @return const float* 
         */
        const float* GetPixelData() const { return m_PixelData; }

        void Release();

    private:
        void LoadVulkanImage();

        void CreateImageView();

        void CreateTextureSampler();

        void CopyBufferToImage(VkBuffer t_Buffer);

        void GenerateMipMaps(VkFormat t_ImageFormat);
		const LogicalDevice* m_Device;
        VkImage m_Image;

        VkImageView m_ImageView;

        VkSampler m_TextureSampler;

        VkDeviceMemory m_Memory;

        VkDescriptorImageInfo m_ImageInfo = {};

        float* m_PixelData;

        VkFormat m_Format = VK_FORMAT_R16G16B16_SFLOAT;

        UINT32 m_Width = 0;

        UINT32 m_Height = 0;

        UINT32 m_MipLevels = 0;

        INT32 m_Channels = 0;
    };
}