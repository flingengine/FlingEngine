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
            std::vector<Shader*> t_Shaders,
            VkDevice t_LogicalDevice,
            VkPolygonMode t_Mode = VK_POLYGON_MODE_FILL,
            Depth t_Depth = Depth::ReadWrite,
            VkPrimitiveTopology t_Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VkCullModeFlags t_CullMode = VK_CULL_MODE_BACK_BIT,
            VkFrontFace t_FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE);

        void BindGraphicsPipeline(const VkCommandBuffer& t_CommandBuffer);
        void CreateGraphicsPipeline(VkRenderPass& t_RenderPass, Multisampler* t_Sampler);

        const std::vector<Shader*> GetShaders() const { return m_Shaders; }

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

        void CreateAttributes(Multisampler* t_Sampler);
        
        std::vector<Shader*> m_Shaders;

        VkDevice m_Device;

        Depth m_Depth;
        VkPrimitiveTopology m_Topology;
        VkPolygonMode m_PolygonMode;
        VkCullModeFlags m_CullMode;
        VkFrontFace m_FrontFace;

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