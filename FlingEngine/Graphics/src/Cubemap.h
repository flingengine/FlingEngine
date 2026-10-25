#pragma once

#include "Resource.h"
#include "FlingVulkan.h"
#include <memory>

namespace Fling
{
    /**
     * A cube map image made from six face images in one folder. The Guid is the folder path
     * relative to the assets directory, for example "Textures/Skybox/Garden". The folder must
     * have the files posx, negx, posy, negy, posz and negz, each with an image extension
     * (.png, .jpg, .jpeg, .tga or .bmp). All faces must be square and the same size.
     */
    class Cubemap : public Resource
    {
    public:
        static std::shared_ptr<Fling::Cubemap> Create(Guid t_ID);

        explicit Cubemap(Guid t_ID);
        virtual ~Cubemap();

        FORCEINLINE uint32 GetFaceSize() const { return m_FaceSize; }

        /** True when the faces loaded and the GPU image exists */
        FORCEINLINE bool IsValid() const { return m_ImageView != VK_NULL_HANDLE; }

        FORCEINLINE const VkDescriptorImageInfo* GetDescriptorInfo() const { return &m_ImageInfo; }

        /** Release the Vulkan resources of this cube map */
        void Release();

    private:
        /** Load the six faces and upload them to one cube compatible image with six layers */
        void LoadVulkanImage();

        void CreateImageView();

        void CreateSampler();

        /** Width and height of each face, in pixels */
        uint32 m_FaceSize = 0;

        VkImage m_Image = VK_NULL_HANDLE;

        VkDeviceMemory m_ImageMemory = VK_NULL_HANDLE;

        VkImageView m_ImageView = VK_NULL_HANDLE;

        VkSampler m_Sampler = VK_NULL_HANDLE;

        VkDescriptorImageInfo m_ImageInfo{};
    };
} // namespace Fling
