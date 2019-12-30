#include "GraphicsPipeline.h"
#include "GraphicsHelpers.h"

namespace Fling
{
    const std::vector<VkDynamicState> DYNAMIC_STATES = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH };

    GraphicsPipeline::GraphicsPipeline(
        std::vector<Shader*> t_Shaders,
        VkDevice t_LogicalDevice,
        VkPolygonMode t_Mode,
        Depth t_Depth,
        VkPrimitiveTopology t_Topology,
        VkCullModeFlags t_CullMode,
        VkFrontFace t_FrontFace) :
        m_Shaders(t_Shaders),
        m_Device(t_LogicalDevice),
        m_PolygonMode(t_Mode),
        m_Depth(t_Depth),
        m_Topology(t_Topology),
        m_CullMode(t_CullMode),
        m_FrontFace(t_FrontFace)
    {
		m_DescriptorSetLayout = Shader::CreateSetLayout(m_Device, m_Shaders);
		m_PipelineLayout = Shader::CreatePipelineLayout(m_Device, m_DescriptorSetLayout, 0, 0);
		
		CreateAttributes(nullptr);
    }

    void GraphicsPipeline::BindGraphicsPipeline(const VkCommandBuffer& t_CommandBuffer)
    {
        vkCmdBindPipeline(t_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
    }

    void GraphicsPipeline::CreateAttributes(Multisampler* t_Sampler)
    {
        // Input Assembly 
        m_InputAssemblyState = Initializers::PipelineInputAssemblyStateCreateInfo(m_Topology, 0, VK_FALSE);

        // Dynamic States
        m_DynamicState = Initializers::PipelineDynamicStateCreateInfo(DYNAMIC_STATES, 0);

        // Rasterization State 
        m_RasterizationState =
            Initializers::PipelineRasterizationStateCreateInfo(
                m_PolygonMode,
                m_CullMode,
                m_FrontFace);

        // Multisampler State (default to 1 bit)
        m_MultisampleState = Initializers::PipelineMultiSampleStateCreateInfo(
			t_Sampler ? t_Sampler->GetSampleCountFlagBits() : VK_SAMPLE_COUNT_1_BIT, 
			0
		);

        // Color Attatchment
		m_ColorBlendAttachmentStates.resize(1);
        m_ColorBlendAttachmentStates[0] = Initializers::PipelineColorBlendAttachmentState(
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            VK_FALSE);

        // Color Blend State
        m_ColorBlendState = Initializers::PipelineColorBlendStateCreateInfo(1, m_ColorBlendAttachmentStates.data());

        // Depth stencil state
        m_DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        m_DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        m_DepthStencilState.front = m_DepthStencilState.back;
        m_DepthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;

        switch (m_Depth) {
        case Depth::None:
            m_DepthStencilState.depthTestEnable = VK_FALSE;
            m_DepthStencilState.depthWriteEnable = VK_FALSE;
            break;
        case Depth::Read:
            m_DepthStencilState.depthTestEnable = VK_TRUE;
            m_DepthStencilState.depthWriteEnable = VK_FALSE;
            break;
        case Depth::Write:
            m_DepthStencilState.depthTestEnable = VK_FALSE;
            m_DepthStencilState.depthWriteEnable = VK_TRUE;
            break;
        case Depth::ReadWrite:
            m_DepthStencilState.depthTestEnable = VK_TRUE;
            m_DepthStencilState.depthWriteEnable = VK_TRUE;
            break;
        }

        //View port
        m_ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        m_ViewportState.viewportCount = 1;
        m_ViewportState.scissorCount = 1;
    }

    void GraphicsPipeline::CreateGraphicsPipeline(VkRenderPass& t_RenderPass, Multisampler* t_Sampler)
    {
        // Pipeline Cache
        GraphicsHelpers::CreatePipelineCache(m_PipelineCache);

        // Shader stages 
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

        for (Shader* shader : m_Shaders)
        {
            VkPipelineShaderStageCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            createInfo.module = shader->GetShaderModule();
            createInfo.stage = shader->GetStage();
            createInfo.pName = "main";
            createInfo.flags = 0;
            createInfo.pNext = nullptr;
            createInfo.pSpecializationInfo = nullptr;
            shaderStages.push_back(createInfo);
        }

        // Vertex Input 
        VkVertexInputBindingDescription BindingDescription = Vertex::GetBindingDescription();
        std::array<VkVertexInputAttributeDescription, 5> AttributeDescriptions = Vertex::GetAttributeDescriptions();

        m_VertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        m_VertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
        m_VertexInputStateCreateInfo.pVertexBindingDescriptions = &BindingDescription;
        m_VertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<UINT32>(AttributeDescriptions.size());
        m_VertexInputStateCreateInfo.pVertexAttributeDescriptions = AttributeDescriptions.data();


        // Create graphics pipeline ------------------------
        m_PipelineCreateInfo = {};
        m_PipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        m_PipelineCreateInfo.stageCount = static_cast<UINT32>(shaderStages.size());
        m_PipelineCreateInfo.pStages = shaderStages.data();
        m_PipelineCreateInfo.pVertexInputState = &m_VertexInputStateCreateInfo;
        m_PipelineCreateInfo.pInputAssemblyState = &m_InputAssemblyState;
        m_PipelineCreateInfo.pViewportState = &m_ViewportState;
        m_PipelineCreateInfo.pRasterizationState = &m_RasterizationState;
        m_PipelineCreateInfo.pMultisampleState = &m_MultisampleState;
        m_PipelineCreateInfo.pDepthStencilState = &m_DepthStencilState;
        m_PipelineCreateInfo.pDynamicState = &m_DynamicState;
        m_PipelineCreateInfo.pColorBlendState = &m_ColorBlendState;
        m_PipelineCreateInfo.layout = m_PipelineLayout;
        m_PipelineCreateInfo.renderPass = t_RenderPass;
        m_PipelineCreateInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(m_Device, m_PipelineCache, 1, &m_PipelineCreateInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to create graphics pipeline");
        }
    }

    GraphicsPipeline::~GraphicsPipeline()
    {
        vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
        vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
        vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);
        vkDestroyPipelineCache(m_Device, m_PipelineCache, nullptr);
    }
}