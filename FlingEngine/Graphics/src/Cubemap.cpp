#include "Cubemap.h"

namespace Fling
{
    Cubemap::Cubemap(
        std::shared_ptr<class Image> t_PosX,
        std::shared_ptr<class Image> t_NegX,
        std::shared_ptr<class Image> t_PosY,
        std::shared_ptr<class Image> t_NegY,
        std::shared_ptr<class Image> t_PosZ,
        std::shared_ptr<class Image> t_NegZ,
        VkRenderPass t_renderPass,
        VkDevice t_logicalDevice) : 
        m_Device(t_logicalDevice),
        m_RenderPass(t_renderPass)
         
    {
        m_UniformBuffer = std::make_unique<Buffer>(
            sizeof(UboVS),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        m_UniformBuffer->MapMemory();

        m_Cube = Model::Create( "Models/cube.obj"_hs);        
    }



    Cubemap::~Cubemap()
    {
        if (m_ImageMemory)
        {
            vkFreeMemory(m_Device, m_ImageMemory, nullptr);
            m_ImageMemory = nullptr;
        }

        vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);
        vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
        vkDestroyPipelineCache(m_Device, m_PipelineCache, nullptr);
        vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
        vkDestroyImage(m_Device, m_Image, nullptr);
        vkDestroyImageView(m_Device, m_Imageview, nullptr);
        vkDestroySampler(m_Device, m_Sampler, nullptr);   
    }

    void Cubemap::Init(std::shared_ptr<Camera> t_camera)
    {
        LoadCubemap();
        UpdateUniformBuffer(t_camera->GetProjectionMatrix(), t_camera->GetRotation());
        SetupDescriptors();
    }

    void Cubemap::PreparePipeline()
    {
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
        if (vkCreatePipelineCache(m_Device, &pipelineCacheCreateInfo, nullptr, &m_PipelineCache) != VK_SUCCESS)
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

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = Initalizers::PipelineCreateInfo(m_PipelineLayout, m_RenderPass, 0);
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

        //TODO: Load in different shaders for different cubemaps
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

        if (vkCreateGraphicsPipelines(m_Device, m_PipelineCache, 1, &pipelineCreateInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
        {
            F_LOG_ERROR("Failed to create graphics pipeline for cube map");
        }

        vkDestroyShaderModule(m_Device, vertModule, nullptr);
        vkDestroyShaderModule(m_Device, fragModule, nullptr);
    }

    void Cubemap::LoadCubemap()
    {
    }

    void Cubemap::SetupDescriptors()
    {
        //Descriptor pools
        std::vector<VkDescriptorPoolSize> poolSizes =
        {
            Initalizers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
            Initalizers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
        };

        VkDescriptorPoolCreateInfo descriptorPoolInfo =
            Initalizers::DescriptorPoolCreateInfo(poolSizes, 2);

        if (vkCreateDescriptorPool(m_Device, &descriptorPoolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
        {
            F_LOG_ERROR("Cube map failed to create descriptor pool");
        }

        //Descriptor SetLayout
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBinding =
        {
            //Binding 0 : vertex shader uniform buffer
            Initalizers::DescriptorSetLayoutBinding(
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT,
                0),

            Initalizers::DescriptorSetLayoutBinding(
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                1),
        };

        VkDescriptorSetLayoutCreateInfo descriptorLayout =
            Initalizers::DescriptorSetLayoutCreateInfo(
                setLayoutBinding);

        if (vkCreateDescriptorSetLayout(m_Device, &descriptorLayout, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
        {
            F_LOG_ERROR("Cube map failed to create descriptor set layout");
        }

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
            Initalizers::PiplineLayoutCreateInfo(
                &m_DescriptorSetLayout,
                1);

        if (vkCreatePipelineLayout(m_Device, &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
        {
            F_LOG_ERROR("Cube map failed to create pipeline layout");
        }

        //Descriptor Sets
        VkDescriptorImageInfo textureDescriptor =
            Initalizers::DescriptorImageInfo(
                m_Sampler,
                m_Imageview,
                m_ImageLayout);

        VkDescriptorSetAllocateInfo allocInfo =
            Initalizers::DescriptorSetAllocateInfo(
                m_DescriptorPool,
                &m_DescriptorSetLayout,
                1);

        if (vkAllocateDescriptorSets(m_Device, &allocInfo, &m_DescriptorSet) != VK_SUCCESS)
        {
            F_LOG_ERROR("Cube map failed to allocate descriptor sets")
        }

        std::vector<VkWriteDescriptorSet> writeDescriptorSets =
        {
            // Binding 0 : Vertex shader uniform buffer
            Initalizers::WriteDescriptorSet(
                    m_DescriptorSet,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    0,
                    &m_UniformBuffer->GetDescriptor()),

            // Binding 1 : Fragment shader cubemap sampler
            Initalizers::WriteDescriptorSet(
                m_DescriptorSet,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                1,
                &textureDescriptor),
        };

        vkUpdateDescriptorSets(m_Device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
    }

    void Cubemap::UpdateUniformBuffer(const glm::mat4& t_projectionMatrix,  glm::vec3 t_rotation)
    {
        m_UboVS.Projection = t_projectionMatrix;
        
        glm::mat4 viewMatrix = glm::mat4(1.0f);
        m_UboVS.ModelView = glm::mat4(1.0f);
        m_UboVS.ModelView = viewMatrix * glm::translate(m_UboVS.ModelView, glm::vec3(0, 0, 0));
        m_UboVS.ModelView = glm::rotate(m_UboVS.ModelView, glm::radians(t_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        m_UboVS.ModelView = glm::rotate(m_UboVS.ModelView, glm::radians(t_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        m_UboVS.ModelView = glm::rotate(m_UboVS.ModelView, glm::radians(t_rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

        memcpy(m_UniformBuffer->m_MappedMem, &m_UboVS, sizeof(m_UboVS));
    }
}