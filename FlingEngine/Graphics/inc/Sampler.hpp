#pragma once

#include "FlingVulkan.h"

namespace Fling
{
    // Hijacked this simple sampler wrapper from Granite:
    // https://github.com/Themaister/Granite
    enum class StockSampler
    {
        NearestClamp,
        LinearClamp,
        TrilinearClamp,
        NearestWrap,
        LinearWrap,
        TrilinearWrap,
        NearestShadow,
        LinearShadow,
        Count
    };

    struct SamplerCreateInfo
    {
        VkFilter mag_filter;
        VkFilter min_filter;
        VkSamplerMipmapMode mipmap_mode;
        VkSamplerAddressMode address_mode_u;
        VkSamplerAddressMode address_mode_v;
        VkSamplerAddressMode address_mode_w;
        float mip_lod_bias;
        VkBool32 anisotropy_enable;
        float max_anisotropy;
        VkBool32 compare_enable;
        VkCompareOp compare_op;
        float min_lod;
        float max_lod;
        VkBorderColor border_color;
        VkBool32 unnormalized_coordinates;
    };

    class Sampler;
    struct SamplerDeleter
    {
        void operator()(Sampler *sampler);
    };

    /**
     * @brief   A sampler is what Vulkan passes to it's shaders 
     *          in order to sample positions of an images
     */
    class Sampler
    {
    public:
        ~Sampler();
        VkSampler GetSampler() const { return m_Sampler; }

        const SamplerCreateInfo &GetCreateInfo() const { return m_CreateInfo; }

    private:
    
        //Sampler(LogicalDevice* device, VkSampler sampler, const SamplerCreateInfo &info);

        //LogicalDevice* m_Device;
        VkSampler m_Sampler;
        SamplerCreateInfo m_CreateInfo;
    };
}   // namespace Fling