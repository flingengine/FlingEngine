#include "Cubemap.h"

Fling::Cubemap::Cubemap(
    std::shared_ptr<class Image> t_PosX, 
    std::shared_ptr<class Image> t_NegX, 
    std::shared_ptr<class Image> t_PosY, 
    std::shared_ptr<class Image> t_NegY, 
    std::shared_ptr<class Image> t_PosZ, 
    std::shared_ptr<class Image> t_NegZ)
{
    void* data;
    UINT32 t_height 
}

Fling::Cubemap::~Cubemap()
{
}

void Fling::Cubemap::PreparePipeline()
{
    VkDevice logicalDevice = m_logicalDevice->GetVkDevice();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        Initalizers::PipelineInputAssemblyStateCreateInfo(
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            0,
            VK_FALSE);

    VkPipelineRasterizationStateCreateInfo rasterizationState =
        Initalizers::PipelineRasterizationStateCreateInfo(
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_BACK_BIT,
            VK_FRONT_FACE_COUNTER_CLOCKWISE,
            0);

    VkPipelineColorBlendAttachmentState blendAttachmentState =
        Initalizers::PipelineColorBlendAttachmentState(
            0xf,
            VK_FALSE);

    VkPipelineColorBlendStateCreateInfo colorBlendState =
        Initalizers::PipelineColorBlendStateCreateInfo(
            1,
            &blendAttachmentState);

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        Initalizers::DepthStencilState(
            VK_FALSE,
            VK_FALSE,
            VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineViewportStateCreateInfo viewportState =
        Initalizers::PipelineViewportStateCreateInfo(
            1, 1, 0);

    VkPipelineMultisampleStateCreateInfo multisampleState =
        Initalizers::PipelineMultiSampleStateCreateInfo(
            VK_SAMPLE_COUNT_1_BIT,
            0);


    std::vector<VkDynamicState> dynamicStateEnables = {
             VK_DYNAMIC_STATE_VIEWPORT,
             VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState =
        Initalizers::PipelineDynamicStateCreateInfo(
            dynamicStateEnables, 0);

    //Pipeline cache 
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (vkCreatePipelineCache(logicalDevice, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache) != VK_SUCCESS)
    {
        F_LOG_ERROR("Could not create pipeline cache for imgui");
    }

    //Vertex input
    VkVertexInputBindingDescription vertexInputBindingDescription = Vertex::GetBindingDescription();
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = Vertex::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo VertexInputState = {};
    VertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VertexInputState.vertexBindingDescriptionCount = 1;
    VertexInputState.pVertexBindingDescriptions = &vertexInputBindingDescription;
    VertexInputState.vertexAttributeDescriptionCount = static_cast<UINT32>(attributeDescriptions.size());;
    VertexInputState.pVertexAttributeDescriptions = attributeDescriptions.data();

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = Initalizers::PipelineCreateInfo(m_pipelineLayout, m_renderPass, 0);
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = shaderStages.size();
    pipelineCreateInfo.pStages = shaderStages.data();
    pipelineCreateInfo.pVertexInputState = &VertexInputState;

    //Load Shader 
    std::shared_ptr<File> vertShaderCode = ResourceManager::Get().LoadResource<File>("Shaders/skybox/skybox.vert.spv");
    assert(vertShaderCode);

    std::shared_ptr<File> fragShaderCode = ResourceManager::Get().LoadResource<File>("Shaders/skybox/skybox.frag.spv");
    assert(fragShaderCode);

    VkShaderModule vertModule = GraphicsHelpers::CreateShaderModule(vertShaderCode);
    VkShaderModule fragModule = GraphicsHelpers::CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragModule;
    fragShaderStageInfo.pName = "main";
    
    shaderStages[0] = vertShaderStageInfo;
    shaderStages[1] = fragShaderStageInfo;

    if (vkCreateGraphicsPipelines(m_logicalDevice->GetVkDevice(), m_pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_pipeline) != VK_SUCCESS)
    {
        F_LOG_ERROR("Could not create graphics pipeline for imgui");
    }

    vkDestroyShaderModule(logicalDevice, vertModule, nullptr);
    vkDestroyShaderModule(logicalDevice, fragModule, nullptr);
}

void Fling::Cubemap::LoadCubemap()
{
}

void Fling::Cubemap::SetupDescriptors()
{
}
