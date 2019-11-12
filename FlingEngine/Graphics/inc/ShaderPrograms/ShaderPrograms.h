#pragma once
#include "FlingVulkan.h"
#include "GraphicsPipeline.h"
#include "Buffer.h"
#include "Image.h"
#include "MultiSampler.h"

#include <vector>

namespace Fling
{
    class ShaderPrograms
    {
    public:
        enum ShaderProgramType
        {
            PBR = 0,
            Reflection = 1,
        };
    public:
        ShaderPrograms(VkDevice t_Device, const std::vector<Shader*>& t_Shaders);
        ~ShaderPrograms();

        void InitGraphicPipeline(VkRenderPass t_Renderpass, Multisampler* t_Sampler);

        const std::shared_ptr<GraphicsPipeline> GetPipeline() const { return m_Pipeline; };
        VkDescriptorSetLayout& GetDescriptorLayout() { return m_DescriptorLayout; }
        VkPipelineLayout& GetPipelineLayout() { return m_PipelineLayout; }

    private:
        VkDescriptorSetLayout m_DescriptorLayout;
        VkPipelineLayout m_PipelineLayout;
        std::shared_ptr<GraphicsPipeline> m_Pipeline;
        std::vector<Shader*> m_Shaders;
        VkDevice m_Device;
    };
} //namespace fling