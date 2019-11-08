#pragma once 
#include "FlingVulkan.h"
#include "Shader.h"
#include "Vertex.h"
#include "MultiSampler.h"

namespace Fling
{
    class GraphicsPipeline
    {
    public:
        enum class Depth
        {
            None = 0,
            Read = 1,
            Write = 2,
            ReadWrite = Read | Write,
        };

        GraphicsPipeline(
            Shader* t_VertexShader,
            Shader* t_FragShader,
            VkDevice t_LogicalDevice,
            VkPolygonMode t_Mode = VK_POLYGON_MODE_FILL,
            Depth t_Depth = Depth::ReadWrite,
            VkPrimitiveTopology t_Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VkCullModeFlags t_CullMode = VK_CULL_MODE_BACK_BIT,
            VkFrontFace t_FrontFace = VK_FRONT_FACE_CLOCKWISE);

        void BindGraphicsPipeline(const VkCommandBuffer& t_CommandBuffer);
        void CreateGraphicsPipeline(VkRenderPass& t_RenderPass, Multisampler* t_Sampler);

        const Shader* GetVertexShader() const { return m_VertexShader; }
        const Shader* GetFragShader() const { return m_FragShader; }

        Depth GetDepth() const { return m_Depth; }
        VkPrimitiveTopology GetTopology() const { return m_Topology; }
        VkPolygonMode GetPolygonMode() const { return m_PolygonMode; }
        VkCullModeFlags GetCullMode() const { return m_CullMode; }
        VkFrontFace GetFrontFace() const { return m_FrontFace; }
        const VkDescriptorSetLayout& GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }
        const VkPipeline& GetPipeline() const { return m_Pipeline; }
        const VkPipelineLayout& GetPipelineLayout() const { return m_PipelineLayout; }
        const VkPipelineBindPoint& GetPipelineBindPoint() const { return m_PipelineBindPoint; }

        ~GraphicsPipeline();

    private:
        void CreateAttributes(Multisampler* t_Sampler);

        VkDevice m_Device;

        Shader* m_VertexShader;
        Shader* m_FragShader;

        Depth m_Depth;
        VkPrimitiveTopology m_Topology;
        VkPolygonMode m_PolygonMode;
        VkCullModeFlags m_CullMode;
        VkFrontFace m_FrontFace;

        std::vector<VkShaderModule> m_Modules;
        std::vector<VkPipelineShaderStageCreateInfo> m_Stages;

        VkPipeline m_Pipeline = VK_NULL_HANDLE;
        VkPipelineCache m_PipelineCache = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipelineBindPoint m_PipelineBindPoint;

        VkDescriptorSetLayout m_DescriptorSetLayout;
        VkPipelineVertexInputStateCreateInfo m_VertexInputStateCreateInfo = {};
        VkPipelineInputAssemblyStateCreateInfo m_InputAssemblyState = {};
        VkPipelineRasterizationStateCreateInfo m_RasterizationState = {};
        std::array<VkPipelineColorBlendAttachmentState, 1> m_ColorBlendAttachmentStates;
        VkPipelineColorBlendStateCreateInfo m_ColorBlendState = {};
        VkPipelineDepthStencilStateCreateInfo m_DepthStencilState = {};
        VkPipelineViewportStateCreateInfo m_ViewportState = {};
        VkPipelineMultisampleStateCreateInfo m_MultisampleState = {};
        VkPipelineDynamicStateCreateInfo m_DynamicState = {};
        VkPipelineTessellationStateCreateInfo m_TessellationState = {};
    };

}