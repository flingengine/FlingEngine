#include "Cubemap.h"
#include "ResourceManager.h"
#include "GraphicsHelpers.h"
#include "LogicalDevice.h"
#include "VulkanApp.h"
#include "Buffer.h"
#include "Logger.h"
#include "stb_image.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <string>

namespace Fling
{
    namespace
    {
        constexpr VkFormat CubemapFormat = VK_FORMAT_R8G8B8A8_UNORM;
        constexpr uint32 CubemapFaceCount = 6;

        /** Face file names in Vulkan cube layer order: +X, -X, +Y, -Y, +Z, -Z */
        constexpr std::array<const char*, CubemapFaceCount> FaceNames = {
            "posx", "negx", "posy", "negy", "posz", "negz"};

        constexpr std::array<const char*, 5> FaceExtensions = {".png", ".jpg", ".jpeg", ".tga", ".bmp"};

        /** Find the file for one face in the folder. Returns an empty string if there is no file. */
        std::string FindFaceFile(const std::string& t_Folder, const char* t_FaceName)
        {
            for (const char* Ext : FaceExtensions)
            {
                std::string Path = t_Folder + "/" + t_FaceName + Ext;
                if (std::filesystem::exists(Path))
                {
                    return Path;
                }
            }
            return {};
        }
    } // namespace

    std::shared_ptr<Fling::Cubemap> Cubemap::Create(Guid t_ID)
    {
        return ResourceManager::LoadResource<Fling::Cubemap>(t_ID);
    }

    Cubemap::Cubemap(Guid t_ID) : Resource(t_ID)
    {
        // Under --null-render there is no GPU device, so there is no image to make
        if (!VulkanApp::Get().HasDevice())
        {
            return;
        }

        LoadVulkanImage();
        if (m_Image == VK_NULL_HANDLE)
        {
            return;
        }

        CreateImageView();
        CreateSampler();

        m_ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_ImageInfo.imageView = m_ImageView;
        m_ImageInfo.sampler = m_Sampler;
    }

    void Cubemap::LoadVulkanImage()
    {
        const std::string Folder = GetFilepathReleativeToAssets();

        // Load every face first, so that a missing or bad face makes no GPU objects
        std::array<stbi_uc*, CubemapFaceCount> Pixels = {};
        auto FreePixels = [&Pixels]()
        {
            for (stbi_uc*& Face : Pixels)
            {
                stbi_image_free(Face);
                Face = nullptr;
            }
        };

        for (uint32 Face = 0; Face < CubemapFaceCount; ++Face)
        {
            const std::string Path = FindFaceFile(Folder, FaceNames[Face]);
            if (Path.empty())
            {
                F_LOG_ERROR("Cubemap {}: cannot find face '{}'", Folder, FaceNames[Face]);
                FreePixels();
                return;
            }

            int Width = 0;
            int Height = 0;
            int Channels = 0;
            Pixels[Face] = stbi_load(Path.c_str(), &Width, &Height, &Channels, STBI_rgb_alpha);
            if (!Pixels[Face])
            {
                F_LOG_ERROR("Cubemap {}: failed to load face {}", Folder, Path);
                FreePixels();
                return;
            }

            if (Width != Height || (Face > 0 && static_cast<uint32>(Width) != m_FaceSize))
            {
                F_LOG_ERROR("Cubemap {}: face {} is {}x{}. All faces must be square and the same size.",
                            Folder,
                            Path,
                            Width,
                            Height);
                FreePixels();
                return;
            }
            m_FaceSize = static_cast<uint32>(Width);
        }

        // Put all six faces one after the other in a staging buffer
        const VkDeviceSize FaceBytes = static_cast<VkDeviceSize>(m_FaceSize) * m_FaceSize * 4;
        Buffer StagingBuffer(FaceBytes * CubemapFaceCount,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        StagingBuffer.MapMemory();
        for (uint32 Face = 0; Face < CubemapFaceCount; ++Face)
        {
            std::memcpy(static_cast<uint8*>(StagingBuffer.m_MappedMem) + FaceBytes * Face, Pixels[Face], FaceBytes);
        }
        StagingBuffer.UnmapMemory();
        FreePixels();

        VkDevice Device = VulkanApp::Get().GetLogicalDevice()->GetVkDevice();
        GraphicsHelpers::CreateVkImage(Device,
                                       m_FaceSize,
                                       m_FaceSize,
                                       /* Mip levels */ 1,
                                       /* Depth */ 1,
                                       /* Array layers */ CubemapFaceCount,
                                       CubemapFormat,
                                       VK_IMAGE_TILING_OPTIMAL,
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                                       m_Image,
                                       m_ImageMemory);

        std::array<VkBufferImageCopy, CubemapFaceCount> Regions = {};
        for (uint32 Face = 0; Face < CubemapFaceCount; ++Face)
        {
            VkBufferImageCopy& Region = Regions[Face];
            Region.bufferOffset = FaceBytes * Face;
            Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            Region.imageSubresource.mipLevel = 0;
            Region.imageSubresource.baseArrayLayer = Face;
            Region.imageSubresource.layerCount = 1;
            Region.imageExtent = {m_FaceSize, m_FaceSize, 1};
        }

        VkImageSubresourceRange Range = {};
        Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Range.baseMipLevel = 0;
        Range.levelCount = 1;
        Range.baseArrayLayer = 0;
        Range.layerCount = CubemapFaceCount;

        VkCommandBuffer CmdBuf = GraphicsHelpers::BeginSingleTimeCommands();

        GraphicsHelpers::SetImageLayout(CmdBuf,
                                        m_Image,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        Range,
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT);

        vkCmdCopyBufferToImage(CmdBuf,
                               StagingBuffer.GetVkBuffer(),
                               m_Image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<uint32>(Regions.size()),
                               Regions.data());

        GraphicsHelpers::SetImageLayout(CmdBuf,
                                        m_Image,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        Range,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        GraphicsHelpers::EndSingleTimeCommands(CmdBuf);
    }

    void Cubemap::CreateImageView()
    {
        VkImageViewCreateInfo ViewInfo = Initializers::ImageViewCreateInfo();
        ViewInfo.image = m_Image;
        ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        ViewInfo.format = CubemapFormat;
        ViewInfo.components = {
            VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
        ViewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, CubemapFaceCount};

        VK_CHECK_RESULT(
            vkCreateImageView(VulkanApp::Get().GetLogicalDevice()->GetVkDevice(), &ViewInfo, nullptr, &m_ImageView));
    }

    void Cubemap::CreateSampler()
    {
        VkSamplerCreateInfo SamplerInfo = Initializers::SamplerCreateInfo();
        SamplerInfo.magFilter = VK_FILTER_LINEAR;
        SamplerInfo.minFilter = VK_FILTER_LINEAR;
        SamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        // Clamp so that the edges between faces do not show seams
        SamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerInfo.compareOp = VK_COMPARE_OP_NEVER;
        SamplerInfo.minLod = 0.0f;
        SamplerInfo.maxLod = 0.0f;
        SamplerInfo.maxAnisotropy = 1.0f;
        SamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        VK_CHECK_RESULT(
            vkCreateSampler(VulkanApp::Get().GetLogicalDevice()->GetVkDevice(), &SamplerInfo, nullptr, &m_Sampler));
    }

    void Cubemap::Release()
    {
        LogicalDevice* LogDevice = VulkanApp::Get().GetLogicalDevice();
        if (LogDevice == nullptr)
        {
            // --null-render: the constructor made no GPU objects
            return;
        }

        VkDevice Device = LogDevice->GetVkDevice();

        if (m_Sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(Device, m_Sampler, nullptr);
            m_Sampler = VK_NULL_HANDLE;
        }
        if (m_ImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(Device, m_ImageView, nullptr);
            m_ImageView = VK_NULL_HANDLE;
        }
        if (m_Image != VK_NULL_HANDLE)
        {
            vkDestroyImage(Device, m_Image, nullptr);
            m_Image = VK_NULL_HANDLE;
        }
        if (m_ImageMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(Device, m_ImageMemory, nullptr);
            m_ImageMemory = VK_NULL_HANDLE;
        }
    }

    Cubemap::~Cubemap()
    {
        Release();
    }
} // namespace Fling
