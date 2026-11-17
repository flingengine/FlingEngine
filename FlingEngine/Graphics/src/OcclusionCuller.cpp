#include "Buffer.h"
#include "GraphicsHelpers.h"
#include "Logger.h"
#include "LogicalDevice.h"
#include "OcclusionCuller.h"
#include "OcclusionTest.h"
#include "PhyscialDevice.h"
#include "Shader.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace Fling
{
    namespace
    {
        constexpr VkFormat PyramidFormat = VK_FORMAT_R32_SFLOAT;
        constexpr uint32 MaxLevels = 16;
        constexpr uint32 CullGroupSize = 64;
        constexpr uint32 DownsampleGroupSize = 8;
        constexpr VkMemoryPropertyFlags HostVisible =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        /** The std140 block Params of cull.comp */
        struct alignas(16) CullParams
        {
            glm::mat4 ViewProjection{1.0f};
            glm::uvec4 Counts{0u};
            /** x: the blend factor */
            glm::vec4 Blend{0.0f};
            glm::vec4 Planes[Frustum::Count] = {};
        };

        /** The push constant block of hiz_downsample.comp */
        struct DownsamplePush
        {
            int32 SrcSize[2];
            int32 DstSize[2];
            int32 Copy;
        };

        VkDescriptorSetLayoutBinding Binding(const uint32 t_Binding, const VkDescriptorType t_Type)
        {
            VkDescriptorSetLayoutBinding B = {};
            B.binding = t_Binding;
            B.descriptorType = t_Type;
            B.descriptorCount = 1;
            B.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            return B;
        }

        void ComputeBarrier(VkCommandBuffer t_Cmd,
                            const VkPipelineStageFlags t_SrcStage,
                            const VkAccessFlags t_SrcAccess,
                            const VkPipelineStageFlags t_DstStage,
                            const VkAccessFlags t_DstAccess)
        {
            VkMemoryBarrier Barrier = {};
            Barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            Barrier.srcAccessMask = t_SrcAccess;
            Barrier.dstAccessMask = t_DstAccess;
            vkCmdPipelineBarrier(t_Cmd, t_SrcStage, t_DstStage, 0, 1, &Barrier, 0, nullptr, 0, nullptr);
        }
    } // namespace

    OcclusionCuller::OcclusionCuller(const LogicalDevice* t_Device,
                                     std::shared_ptr<Shader> t_Downsample,
                                     std::shared_ptr<Shader> t_Cull)
        : m_Device(t_Device)
    {
        assert(m_Device);
        if (!t_Downsample || !t_Cull || t_Downsample->GetShaderModule() == VK_NULL_HANDLE ||
            t_Cull->GetShaderModule() == VK_NULL_HANDLE)
        {
            F_LOG_WARN("Occlusion culling is off: its compute shaders did not load");
            return;
        }

        const VkDevice Dev = m_Device->GetVkDevice();
        CreatePipelines(t_Downsample, t_Cull);

        VkSamplerCreateInfo SamplerInfo = {};
        SamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        SamplerInfo.magFilter = VK_FILTER_NEAREST;
        SamplerInfo.minFilter = VK_FILTER_NEAREST;
        SamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        SamplerInfo.addressModeU = SamplerInfo.addressModeV = SamplerInfo.addressModeW =
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerInfo.maxLod = static_cast<float>(MaxLevels);
        VK_CHECK_RESULT(vkCreateSampler(Dev, &SamplerInfo, nullptr, &m_Sampler));

        const std::array<VkDescriptorPoolSize, 4> PoolSizes = {
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MaxLevels + 2},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MaxLevels},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6}};
        VkDescriptorPoolCreateInfo PoolInfo = {};
        PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        PoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        PoolInfo.maxSets = MaxLevels + 1;
        PoolInfo.poolSizeCount = static_cast<uint32>(PoolSizes.size());
        PoolInfo.pPoolSizes = PoolSizes.data();
        VK_CHECK_RESULT(vkCreateDescriptorPool(Dev, &PoolInfo, nullptr, &m_Pool));

        VkDescriptorSetAllocateInfo Alloc = {};
        Alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        Alloc.descriptorPool = m_Pool;
        Alloc.descriptorSetCount = 1;
        Alloc.pSetLayouts = &m_CullSetLayout;
        VK_CHECK_RESULT(vkAllocateDescriptorSets(Dev, &Alloc, &m_CullSet));

        m_Params = new Buffer(sizeof(CullParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, HostVisible);
        m_Params->MapMemory(sizeof(CullParams));
        m_Stats = new Buffer(
            sizeof(uint32) * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, HostVisible);
        m_Stats->MapMemory(sizeof(uint32) * 4);
        std::memset(m_Stats->m_MappedMem, 0, sizeof(uint32) * 4);

        m_Supported = true;
    }

    OcclusionCuller::~OcclusionCuller()
    {
        const VkDevice Dev = m_Device->GetVkDevice();
        DestroyPyramid();
        for (Buffer* B : m_Retired)
        {
            delete B;
        }
        delete m_Params;
        delete m_Slots;
        delete m_Draw2;
        delete m_Visible;
        delete m_Stats;
        if (m_Sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(Dev, m_Sampler, nullptr);
        }
        if (m_Pool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(Dev, m_Pool, nullptr);
        }
        if (m_CullPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(Dev, m_CullPipeline, nullptr);
        if (m_DownsamplePipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(Dev, m_DownsamplePipeline, nullptr);
        if (m_CullPipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(Dev, m_CullPipelineLayout, nullptr);
        if (m_DownsamplePipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(Dev, m_DownsamplePipelineLayout, nullptr);
        if (m_CullSetLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(Dev, m_CullSetLayout, nullptr);
        if (m_DownsampleSetLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(Dev, m_DownsampleSetLayout, nullptr);
    }

    void OcclusionCuller::CreatePipelines(const std::shared_ptr<Shader>& t_Downsample,
                                          const std::shared_ptr<Shader>& t_Cull)
    {
        const VkDevice Dev = m_Device->GetVkDevice();

        auto MakeLayout = [&](const std::vector<VkDescriptorSetLayoutBinding>& t_Bindings,
                              const uint32 t_PushSize,
                              VkDescriptorSetLayout& t_SetLayout,
                              VkPipelineLayout& t_PipelineLayout)
        {
            VkDescriptorSetLayoutCreateInfo SetInfo = {};
            SetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            SetInfo.bindingCount = static_cast<uint32>(t_Bindings.size());
            SetInfo.pBindings = t_Bindings.data();
            VK_CHECK_RESULT(vkCreateDescriptorSetLayout(Dev, &SetInfo, nullptr, &t_SetLayout));

            VkPushConstantRange Push = {VK_SHADER_STAGE_COMPUTE_BIT, 0, t_PushSize};
            VkPipelineLayoutCreateInfo LayoutInfo = {};
            LayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            LayoutInfo.setLayoutCount = 1;
            LayoutInfo.pSetLayouts = &t_SetLayout;
            LayoutInfo.pushConstantRangeCount = 1;
            LayoutInfo.pPushConstantRanges = &Push;
            VK_CHECK_RESULT(vkCreatePipelineLayout(Dev, &LayoutInfo, nullptr, &t_PipelineLayout));
        };

        auto MakePipeline = [&](const std::shared_ptr<Shader>& t_Shader, VkPipelineLayout t_Layout, VkPipeline& t_Out)
        {
            VkComputePipelineCreateInfo Info = {};
            Info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            Info.layout = t_Layout;
            Info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            Info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            Info.stage.module = t_Shader->GetShaderModule();
            Info.stage.pName = "main";
            VK_CHECK_RESULT(vkCreateComputePipelines(Dev, VK_NULL_HANDLE, 1, &Info, nullptr, &t_Out));
        };

        MakeLayout({Binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER),
                    Binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                    Binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                    Binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                    Binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                    Binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                    Binding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER),
                    Binding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)},
                   sizeof(uint32),
                   m_CullSetLayout,
                   m_CullPipelineLayout);
        MakePipeline(t_Cull, m_CullPipelineLayout, m_CullPipeline);

        MakeLayout(
            {Binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER), Binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)},
            sizeof(DownsamplePush),
            m_DownsampleSetLayout,
            m_DownsamplePipelineLayout);
        MakePipeline(t_Downsample, m_DownsamplePipelineLayout, m_DownsamplePipeline);
    }

    void OcclusionCuller::DestroyPyramid()
    {
        const VkDevice Dev = m_Device->GetVkDevice();
        if (!m_DownsampleSets.empty())
        {
            vkFreeDescriptorSets(Dev, m_Pool, static_cast<uint32>(m_DownsampleSets.size()), m_DownsampleSets.data());
            m_DownsampleSets.clear();
        }
        for (VkImageView View : m_LevelViews)
        {
            vkDestroyImageView(Dev, View, nullptr);
        }
        m_LevelViews.clear();
        if (m_PyramidAllLevels != VK_NULL_HANDLE)
        {
            vkDestroyImageView(Dev, m_PyramidAllLevels, nullptr);
            m_PyramidAllLevels = VK_NULL_HANDLE;
        }
        if (m_PyramidImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(Dev, m_PyramidImage, nullptr);
            m_PyramidImage = VK_NULL_HANDLE;
        }
        if (m_PyramidMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(Dev, m_PyramidMemory, nullptr);
            m_PyramidMemory = VK_NULL_HANDLE;
        }
    }

    void OcclusionCuller::SetDepth(const VkImageView t_DepthView,
                                   const uint32 t_Width,
                                   const uint32 t_Height,
                                   const VkFormat t_DepthFormat)
    {
        if (!m_Supported)
        {
            return;
        }
        const PhysicalDevice* Phys = m_Device->GetPhysicalDevice();
        const bool bSampleDepth =
            (Phys->GetFormatProperties(t_DepthFormat).optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
        const bool bStorage =
            (Phys->GetFormatProperties(PyramidFormat).optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
        const uint32 Levels = DepthPyramid::LevelsFor(t_Width, t_Height);
        if (!bSampleDepth || !bStorage || Levels > MaxLevels)
        {
            F_LOG_WARN("Occlusion culling is off: the device cannot sample the depth format or store the pyramid");
            m_Supported = false;
            return;
        }

        const VkDevice Dev = m_Device->GetVkDevice();
        DestroyPyramid();
        m_Width = t_Width;
        m_Height = t_Height;
        m_DepthView = t_DepthView;

        GraphicsHelpers::CreateVkImage(Dev,
                                       t_Width,
                                       t_Height,
                                       Levels,
                                       1,
                                       1,
                                       PyramidFormat,
                                       VK_IMAGE_TILING_OPTIMAL,
                                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       0,
                                       m_PyramidImage,
                                       m_PyramidMemory,
                                       VK_SAMPLE_COUNT_1_BIT);

        auto MakeView = [&](const uint32 t_Base, const uint32 t_Count)
        {
            VkImageViewCreateInfo Info = {};
            Info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            Info.image = m_PyramidImage;
            Info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            Info.format = PyramidFormat;
            Info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, t_Base, t_Count, 0, 1};
            VkImageView View = VK_NULL_HANDLE;
            VK_CHECK_RESULT(vkCreateImageView(Dev, &Info, nullptr, &View));
            return View;
        };
        m_PyramidAllLevels = MakeView(0, Levels);
        for (uint32 L = 0; L < Levels; ++L)
        {
            m_LevelViews.push_back(MakeView(L, 1));
        }

        // One set per level: the source (the depth, then the level above) and the level it writes
        std::vector<VkDescriptorSetLayout> Layouts(Levels, m_DownsampleSetLayout);
        m_DownsampleSets.resize(Levels);
        VkDescriptorSetAllocateInfo Alloc = {};
        Alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        Alloc.descriptorPool = m_Pool;
        Alloc.descriptorSetCount = Levels;
        Alloc.pSetLayouts = Layouts.data();
        VK_CHECK_RESULT(vkAllocateDescriptorSets(Dev, &Alloc, m_DownsampleSets.data()));

        std::vector<VkDescriptorImageInfo> ImageInfos(size_t{Levels} * 2);
        std::vector<VkWriteDescriptorSet> Writes;
        Writes.reserve(size_t{Levels} * 2);
        for (uint32 L = 0; L < Levels; ++L)
        {
            VkDescriptorImageInfo& Src = ImageInfos[size_t{L} * 2];
            Src.sampler = m_Sampler;
            Src.imageView = L == 0 ? m_DepthView : m_LevelViews[L - 1];
            Src.imageLayout = L == 0 ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
            VkDescriptorImageInfo& Dst = ImageInfos[size_t{L} * 2 + 1];
            Dst.sampler = VK_NULL_HANDLE;
            Dst.imageView = m_LevelViews[L];
            Dst.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkWriteDescriptorSet Write = {};
            Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            Write.dstSet = m_DownsampleSets[L];
            Write.descriptorCount = 1;
            Write.dstBinding = 0;
            Write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            Write.pImageInfo = &Src;
            Writes.push_back(Write);
            Write.dstBinding = 1;
            Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            Write.pImageInfo = &Dst;
            Writes.push_back(Write);
        }
        vkUpdateDescriptorSets(Dev, static_cast<uint32>(Writes.size()), Writes.data(), 0, nullptr);

        WriteCullDescriptors();
    }

    bool OcclusionCuller::EnsureCapacity(const size_t t_Slots, Buffer* t_Draw1)
    {
        if (!m_Supported)
        {
            return false;
        }
        if (m_Slots != nullptr && m_Capacity >= t_Slots && m_Draw1 == t_Draw1)
        {
            return false;
        }

        if (m_Capacity < t_Slots || m_Slots == nullptr)
        {
            const size_t Capacity = std::max<size_t>(t_Slots, 64);
            for (Buffer* Old : {m_Slots, m_Draw2, m_Visible})
            {
                if (Old != nullptr)
                {
                    m_Retired.push_back(Old);
                }
            }
            m_Slots = new Buffer(sizeof(OcclusionSlot) * Capacity, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, HostVisible);
            m_Slots->MapMemory(sizeof(OcclusionSlot) * Capacity);
            std::memset(m_Slots->m_MappedMem, 0, sizeof(OcclusionSlot) * Capacity);
            m_Draw2 = new Buffer(sizeof(VkDrawIndexedIndirectCommand) * Capacity,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                 HostVisible);
            m_Draw2->MapMemory(sizeof(VkDrawIndexedIndirectCommand) * Capacity);
            std::memset(m_Draw2->m_MappedMem, 0, sizeof(VkDrawIndexedIndirectCommand) * Capacity);
            // Nothing was visible yet, so the first frame draws everything in the second pass
            m_Visible = new Buffer(sizeof(uint32) * Capacity, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, HostVisible);
            m_Visible->MapMemory(sizeof(uint32) * Capacity);
            std::memset(m_Visible->m_MappedMem, 0, sizeof(uint32) * Capacity);
            m_Capacity = Capacity;
        }
        m_Draw1 = t_Draw1;
        WriteCullDescriptors();
        return true;
    }

    void OcclusionCuller::ReleaseRetired()
    {
        for (Buffer* B : m_Retired)
        {
            delete B;
        }
        m_Retired.clear();
    }

    void OcclusionCuller::WriteCullDescriptors() const
    {
        if (m_Slots == nullptr || m_Draw1 == nullptr || m_Worlds == nullptr || m_PyramidAllLevels == VK_NULL_HANDLE)
        {
            return;
        }
        const VkDescriptorBufferInfo Infos[7] = {{m_Params->GetVkBuffer(), 0, sizeof(CullParams)},
                                                 {m_Slots->GetVkBuffer(), 0, VK_WHOLE_SIZE},
                                                 {m_Draw1->GetVkBuffer(), 0, VK_WHOLE_SIZE},
                                                 {m_Draw2->GetVkBuffer(), 0, VK_WHOLE_SIZE},
                                                 {m_Visible->GetVkBuffer(), 0, VK_WHOLE_SIZE},
                                                 {m_Stats->GetVkBuffer(), 0, VK_WHOLE_SIZE},
                                                 {m_Worlds->GetVkBuffer(), 0, VK_WHOLE_SIZE}};
        const VkDescriptorImageInfo Pyramid = {m_Sampler, m_PyramidAllLevels, VK_IMAGE_LAYOUT_GENERAL};

        std::array<VkWriteDescriptorSet, 8> Writes = {};
        for (uint32 i = 0; i < 6; ++i)
        {
            Writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            Writes[i].dstSet = m_CullSet;
            Writes[i].dstBinding = i;
            Writes[i].descriptorCount = 1;
            Writes[i].descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            Writes[i].pBufferInfo = &Infos[i];
        }
        Writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        Writes[6].dstSet = m_CullSet;
        Writes[6].dstBinding = 6;
        Writes[6].descriptorCount = 1;
        Writes[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        Writes[6].pImageInfo = &Pyramid;
        Writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        Writes[7].dstSet = m_CullSet;
        Writes[7].dstBinding = 7;
        Writes[7].descriptorCount = 1;
        Writes[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        Writes[7].pBufferInfo = &Infos[6];
        vkUpdateDescriptorSets(m_Device->GetVkDevice(), static_cast<uint32>(Writes.size()), Writes.data(), 0, nullptr);
    }

    OcclusionSlot* OcclusionCuller::MappedSlots() const
    {
        return m_Slots != nullptr ? static_cast<OcclusionSlot*>(m_Slots->m_MappedMem) : nullptr;
    }

    uint32 OcclusionCuller::ReadOccludedCount() const
    {
        return m_Stats != nullptr ? *static_cast<const uint32*>(m_Stats->m_MappedMem) : 0;
    }

    uint32 OcclusionCuller::ReadFrustumCulledCount() const
    {
        return m_Stats != nullptr ? static_cast<const uint32*>(m_Stats->m_MappedMem)[1] : 0;
    }

    void OcclusionCuller::SetWorlds(Buffer* t_Worlds)
    {
        if (m_Worlds != t_Worlds)
        {
            m_Worlds = t_Worlds;
            WriteCullDescriptors();
        }
    }

    void OcclusionCuller::WriteParams(const glm::mat4& t_ViewProjection,
                                      const Frustum& t_View,
                                      const uint32 t_SlotCount,
                                      const float t_BlendFactor)
    {
        m_SlotCount = t_SlotCount;
        CullParams Params;
        Params.ViewProjection = t_ViewProjection;
        Params.Counts = glm::uvec4(t_SlotCount, 0u, 0u, 0u);
        Params.Blend = glm::vec4(t_BlendFactor, 0.0f, 0.0f, 0.0f);
        for (uint32 i = 0; i < Frustum::Count; ++i)
        {
            Params.Planes[i] = t_View.Planes[i];
        }
        std::memcpy(m_Params->m_MappedMem, &Params, sizeof(Params));
    }

    void OcclusionCuller::Dispatch(VkCommandBuffer t_Cmd, const uint32 t_Phase) const
    {
        vkCmdBindPipeline(t_Cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_CullPipeline);
        vkCmdBindDescriptorSets(
            t_Cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_CullPipelineLayout, 0, 1, &m_CullSet, 0, nullptr);
        vkCmdPushConstants(t_Cmd, m_CullPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32), &t_Phase);
        // The recording holds the group count, so it covers the capacity. The shader stops at the slot count.
        vkCmdDispatch(t_Cmd, static_cast<uint32>((m_Capacity + CullGroupSize - 1) / CullGroupSize), 1, 1);
    }

    void OcclusionCuller::RecordFirstPhase(VkCommandBuffer t_Cmd) const
    {
        // The pyramid is made again each frame, so its old contents do not matter. Phase 0 does not
        // read it, but the descriptor set that it binds holds it, so it must be in its layout by then.
        VkImageMemoryBarrier ToGeneral = {};
        ToGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        ToGeneral.srcAccessMask = 0;
        ToGeneral.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        ToGeneral.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ToGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        ToGeneral.srcQueueFamilyIndex = ToGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ToGeneral.image = m_PyramidImage;
        ToGeneral.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, static_cast<uint32>(m_LevelViews.size()), 0, 1};
        vkCmdPipelineBarrier(t_Cmd,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &ToGeneral);

        // The count of hidden meshes starts at 0 for this frame
        vkCmdFillBuffer(t_Cmd, m_Stats->GetVkBuffer(), 0, VK_WHOLE_SIZE, 0);
        ComputeBarrier(t_Cmd,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

        Dispatch(t_Cmd, 0);

        // The first render pass reads the commands, and its vertex shader reads the world matrices that
        // this phase made. Phase 1 reads the visible flags that this phase read, and the matrices.
        ComputeBarrier(t_Cmd,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    }

    void OcclusionCuller::RecordSecondPhase(VkCommandBuffer t_Cmd) const
    {
        vkCmdBindPipeline(t_Cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_DownsamplePipeline);
        uint32 SrcW = m_Width;
        uint32 SrcH = m_Height;
        for (uint32 L = 0; L < m_LevelViews.size(); ++L)
        {
            const uint32 DstW = L == 0 ? m_Width : DepthPyramid::NextSize(SrcW);
            const uint32 DstH = L == 0 ? m_Height : DepthPyramid::NextSize(SrcH);
            DownsamplePush Push = {};
            Push.SrcSize[0] = static_cast<int32>(SrcW);
            Push.SrcSize[1] = static_cast<int32>(SrcH);
            Push.DstSize[0] = static_cast<int32>(DstW);
            Push.DstSize[1] = static_cast<int32>(DstH);
            Push.Copy = L == 0 ? 1 : 0;
            vkCmdBindDescriptorSets(t_Cmd,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    m_DownsamplePipelineLayout,
                                    0,
                                    1,
                                    &m_DownsampleSets[L],
                                    0,
                                    nullptr);
            vkCmdPushConstants(t_Cmd, m_DownsamplePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &Push);
            vkCmdDispatch(t_Cmd,
                          (DstW + DownsampleGroupSize - 1) / DownsampleGroupSize,
                          (DstH + DownsampleGroupSize - 1) / DownsampleGroupSize,
                          1);
            ComputeBarrier(t_Cmd,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_ACCESS_SHADER_READ_BIT);
            SrcW = DstW;
            SrcH = DstH;
        }

        Dispatch(t_Cmd, 1);

        // The second render pass reads the commands, and the CPU reads the count next frame
        ComputeBarrier(t_Cmd,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_HOST_BIT,
                       VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT);
    }
} // namespace Fling
