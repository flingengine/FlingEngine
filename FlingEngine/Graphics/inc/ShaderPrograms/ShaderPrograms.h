#pragma once
#include "FlingVulkan.h"
#include "GraphicsPipeline.h"
#include "Buffer.h"
#include "Image.h"
#include "MultiSampler.h"

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
        ShaderPrograms(VkDevice t_Device, Guid t_VertexShader, Guid t_FragShader);
        void InitGraphicPipeline(VkRenderPass t_Renderpass, Multisampler* t_Sampler);

        std::unique_ptr<GraphicsPipeline> GetPipeline() { return std::unique_ptr<GraphicsPipeline>(m_Pipeline); };
        VkDescriptorSetLayout& GetDescriptorLayout() { return m_DescriptorLayout; }
        VkPipelineLayout& GetPipelineLayout() { return m_PipelineLayout; }

    private:
        VkDescriptorSetLayout m_DescriptorLayout;
        VkPipelineLayout m_PipelineLayout;
        GraphicsPipeline* m_Pipeline = nullptr;
        Shader* m_VertexShader;
        Shader* m_FragShader;
        VkDevice m_Device;
    };
} //namespace fling