#include "GraphicsPipeline.h"

namespace Fling
{
    const std::vector<VkDynamicState> DYNAMIC_STATES = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH };

    GraphicsPipeline::GraphicsPipeline(
        Shader* t_VertexShader,
        Shader* t_FragShader,
        VkPolygonMode t_Mode,
        Depth t_Depth,
        VkPrimitiveTopology t_Topology,
        VkCullModeFlags t_CullMode,
        VkFrontFace t_FrontFace) :
        m_VertexShader(t_VertexShader),
        m_FragShader(t_FragShader),
        m_PolygonMode(t_Mode),
        m_Depth(t_Depth),
        m_Topology(t_Topology),
        m_CullMode(t_CullMode)
    {
        m_Device = Renderer::Get().GetLogicalDevice()->GetVkDevice();
    }

    void GraphicsPipeline::BindGraphicsPipeline(const VkCommandBuffer& t_CommandBuffer)
    {
        vkCmdBindPipeline(t_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
    }

    void GraphicsPipeline::CreateGraphicsPipeline()
    {
        // Input Assembly 
        m_InputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        m_InputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        m_InputAssemblyState.primitiveRestartEnable = VK_FALSE;

        // Dynamic States
        m_DynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        m_DynamicState.dynamicStateCount = static_cast<UINT32>(DYNAMIC_STATES.size());
        m_DynamicState.pDynamicStates = DYNAMIC_STATES.data();

        // Rasterization State
        m_RasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        m_RasterizationState.depthClampEnable = VK_FALSE;
        m_RasterizationState.rasterizerDiscardEnable = VK_FALSE;  // Useful for shadow maps, using would require enabling a GPU feature
        m_RasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
        m_RasterizationState.lineWidth = 1.0f;
        m_RasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
        m_RasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Specify the vertex order! 
        m_RasterizationState.depthBiasEnable = VK_FALSE;
        m_RasterizationState.depthBiasConstantFactor = 0.0f;  // Optional
        m_RasterizationState.depthBiasClamp = 0.0f;           // Optional
        m_RasterizationState.depthBiasSlopeFactor = 0.0f;     // Optional

        // Multisample State 
        // TODO :: MSAA
        m_MultisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        m_MultisampleState.sampleShadingEnable = VK_TRUE;
        m_MultisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        m_MultisampleState.minSampleShading = 0.2f;
        m_MultisampleState.pSampleMask = nullptr; // Optional
        m_MultisampleState.alphaToCoverageEnable = VK_FALSE; // Optional
        m_MultisampleState.alphaToOneEnable = VK_FALSE; // Optional

        // Color Attatchment
        m_ColorBlendAttachmentStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        m_ColorBlendAttachmentStates[0].blendEnable = VK_FALSE;
        m_ColorBlendAttachmentStates[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;     // Optional
        m_ColorBlendAttachmentStates[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;    // Optional
        m_ColorBlendAttachmentStates[0].colorBlendOp = VK_BLEND_OP_ADD;                // Optional
        m_ColorBlendAttachmentStates[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;     // Optional
        m_ColorBlendAttachmentStates[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;    // Optional
        m_ColorBlendAttachmentStates[0].alphaBlendOp = VK_BLEND_OP_ADD;                // Optional

        // Color Blend State
        m_ColorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        m_ColorBlendState.logicOpEnable = VK_FALSE;
        m_ColorBlendState.logicOp = VK_LOGIC_OP_COPY;   // Optional
        m_ColorBlendState.attachmentCount = 1;
        m_ColorBlendState.pAttachments = m_ColorBlendAttachmentStates.data();
        m_ColorBlendState.blendConstants[0] = 0.0f;     // Optional
        m_ColorBlendState.blendConstants[1] = 0.0f;     // Optional
        m_ColorBlendState.blendConstants[2] = 0.0f;     // Optional
        m_ColorBlendState.blendConstants[3] = 0.0f;     // Optional

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

    void GraphicsPipeline::CreateAttributes()
    {
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        std::vector<Shader*> shaders;

        for (const Shader* shader : shaders)
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

    }

    GraphicsPipeline::~GraphicsPipeline()
    {
    }
}