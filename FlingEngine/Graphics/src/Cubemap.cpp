#include "Cubemap.h"
#include "ResourceManager.h"
#include "GraphicsHelpers.h"

namespace Fling
{
    Cubemap::Cubemap(
        Guid t_PosX_ID,
        Guid t_NegX_ID,
        Guid t_PosY_ID,
        Guid t_NegY_ID,
        Guid t_PosZ_ID,
        Guid t_NegZ_ID,
        Guid t_VertexShader,
        Guid t_FragShader,
        VkRenderPass t_RenderPass,
        VkDevice t_LogicalDevice) : 
        m_VertexShader(t_VertexShader), 
        m_FragShader(t_FragShader), 
        m_Device(t_LogicalDevice), 
        m_RenderPass(t_RenderPass)
    {
        //Default format
        m_Format = VK_FORMAT_R8G8B8A8_UNORM;
        m_Cube = Model::Create("Models/cube.obj"_hs);   

        LoadCubemap(
            t_PosX_ID,
            t_NegX_ID,
            t_PosY_ID,
            t_NegY_ID,
            t_PosZ_ID,
            t_NegZ_ID);
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

    void Cubemap::Init(Camera* t_Camera, UINT32 t_CurrentImage, size_t t_NumFramesInFlight)
    {
        // Initialize uniform buffers
        m_numsFrameInFlight = t_NumFramesInFlight;
        m_UniformBuffers.resize(m_numsFrameInFlight);
        for (size_t i = 0; i < m_numsFrameInFlight; ++i)
        {
            m_UniformBuffers[i] = std::make_unique<Buffer>(
                sizeof(UboSkyboxVS),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            m_UniformBuffers[i]->MapMemory();
        }


        UpdateUniformBuffer(t_CurrentImage, t_Camera->GetProjectionMatrix(), t_Camera->GetViewMatrix());
        SetupDescriptors();
        PreparePipeline();
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
                VK_CULL_MODE_FRONT_BIT,
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
        VkVertexInputBindingDescription BindingDescription = Vertex::GetBindingDescription();
        auto AttributeDescriptions = Vertex::GetAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo VertexInputState = {};
        VertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VertexInputState.vertexBindingDescriptionCount = 1;
        VertexInputState.pVertexBindingDescriptions = &BindingDescription;
        VertexInputState.vertexAttributeDescriptionCount = static_cast<UINT32>(AttributeDescriptions.size());;
        VertexInputState.pVertexAttributeDescriptions = AttributeDescriptions.data();

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

        std::shared_ptr<File> vertShaderCode = ResourceManager::LoadResource<File>(m_VertexShader);
        assert(vertShaderCode);

        std::shared_ptr<File> fragShaderCode = ResourceManager::LoadResource<File>(m_FragShader);
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

    void Cubemap::LoadCubemap(
        Guid t_PosX_ID, 
        Guid t_NegX_ID, 
        Guid t_PosY_ID, 
        Guid t_NegY_ID, 
        Guid t_PosZ_ID, 
        Guid t_NegZ_ID)
    {
        std::array<std::shared_ptr<Image>, 6> images =
        {
            ResourceManager::LoadResource<Image>(t_PosX_ID),
            ResourceManager::LoadResource<Image>(t_NegX_ID),
            ResourceManager::LoadResource<Image>(t_PosY_ID),
            ResourceManager::LoadResource<Image>(t_NegY_ID),
            ResourceManager::LoadResource<Image>(t_PosZ_ID),
            ResourceManager::LoadResource<Image>(t_NegZ_ID),
        };

        m_ImageSize = images[0]->GetImageSize() * 6;
        m_NumChannels = images[0]->GetChannels();
        m_LayerSize = m_ImageSize / 6;
        //TODO: add mip levels to image
        m_MipLevels = 1;

        std::unique_ptr<Buffer> stagingBuffer = std::make_unique<Buffer>();
        stagingBuffer->CreateBuffer(m_ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_SHARING_MODE_EXCLUSIVE, true);
        stagingBuffer->MapMemory();

        stbi_uc* pixelDst = (stbi_uc*)(stagingBuffer->m_MappedMem);
        for (size_t i = 0; i < 6; i++)
        {
            const stbi_uc* pixels = images[i]->GetPixelData();
            memcpy(pixelDst, pixels, m_LayerSize);
            pixelDst += m_LayerSize;
        }

        stagingBuffer->UnmapMemory();

        GraphicsHelpers::CreateVkImage(
            images[0]->GetWidth(),
            images[0]->GetHeight(),
            m_MipLevels, // MipLevels
            1, // Depth
            6, // Array layers
            m_Format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
            m_Image,
            m_ImageMemory);

        VkCommandBuffer copyCmd = GraphicsHelpers::BeginSingleTimeCommands();

        std::vector< VkBufferImageCopy> bufferCopyRegions;
        VkDeviceSize offset = 0;

        for (size_t face = 0; face < 6; face++)
        {
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = 0;
            bufferCopyRegion.imageSubresource.baseArrayLayer = face;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = images[face]->GetWidth();
            bufferCopyRegion.imageExtent.height = images[face]->GetHeight();
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;

            bufferCopyRegions.push_back(bufferCopyRegion);

            // Increase offset into staging buffer for next level / face
            offset += images[face]->GetImageSize();
        }

        // Image barrier for optimal image (target)
        // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = m_MipLevels;
        subresourceRange.layerCount = 6;

        GraphicsHelpers::SetImageLayout(
            copyCmd,
            m_Image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange, 
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        // Copy the cube map faces from the staging buffer to the optimal tiled image
        vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer->GetVkBuffer(),
            m_Image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data()
        );

        // Change texture image layout to shader read after all faces have been copied
        m_ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        GraphicsHelpers::SetImageLayout(
            copyCmd,
            m_Image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            m_ImageLayout,
            subresourceRange,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        GraphicsHelpers::EndSingleTimeCommands(copyCmd);

        // Create sampler
        VkSamplerCreateInfo sampler = Initalizers::SamplerCreateInfo();
        sampler.magFilter = VK_FILTER_LINEAR;
        sampler.minFilter = VK_FILTER_LINEAR;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.addressModeV = sampler.addressModeU;
        sampler.addressModeW = sampler.addressModeU;
        sampler.mipLodBias = 0.0f;
        sampler.compareOp = VK_COMPARE_OP_NEVER;
        sampler.minLod = 0.0f;
        sampler.maxLod = static_cast<float>(m_MipLevels);
        sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sampler.maxAnisotropy = 1.0f;
        /*if (m_Device->features.samplerAnisotropy)
        {
            sampler.maxAnisotropy = vulkanDevice->properties.limits.maxSamplerAnisotropy;
            sampler.anisotropyEnable = VK_TRUE;
        }*/
        if (vkCreateSampler(m_Device, &sampler, nullptr, &m_Sampler) != VK_SUCCESS)
        {
            F_LOG_ERROR("Cube failed to create sampler");
        }

        VkImageViewCreateInfo view = Initalizers::ImageViewCreateInfo();
        view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        view.format = m_Format;
        view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        view.subresourceRange.layerCount = 6;
        view.subresourceRange.levelCount = m_MipLevels;
        view.image = m_Image;
        if (vkCreateImageView(m_Device, &view, nullptr, &m_Imageview) != VK_SUCCESS)
        {
            F_LOG_ERROR("Cube failed to create image view");
        }
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

        for (size_t i = 0; i < m_numsFrameInFlight; ++i)
        {
            m_UniformBufferDescriptor = {};
            m_UniformBufferDescriptor.buffer = m_UniformBuffers[i]->GetVkBuffer();
            m_UniformBufferDescriptor.offset = 0;
            m_UniformBufferDescriptor.range = VK_WHOLE_SIZE;

            std::vector<VkWriteDescriptorSet> writeDescriptorSets =
            {
                // Binding 0 : Vertex shader uniform buffer
                Initalizers::WriteDescriptorSet(
                    m_DescriptorSet,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    0,
                    &m_UniformBufferDescriptor),

                // Binding 1 : Fragment shader cubemap sampler
                Initalizers::WriteDescriptorSet(
                    m_DescriptorSet,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    1,
                    &textureDescriptor),
            };

            vkUpdateDescriptorSets(m_Device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
        }
    }

    void Cubemap::UpdateUniformBuffer(UINT32 t_CurrentImage, const glm::mat4& t_ProjectionMatrix, const glm::mat4& t_ViewMatrix)
    {
        m_UboVS.Projection = t_ProjectionMatrix;
        m_UboVS.Projection[1][1] *= -1.0f;

        m_UboVS.ModelView = t_ViewMatrix;
        m_UboVS.ModelView[3][0] = 0.0f;
        m_UboVS.ModelView[3][1] = 0.0f;
        m_UboVS.ModelView[3][2] = 0.0f;

        memcpy(m_UniformBuffers[t_CurrentImage]->m_MappedMem, &m_UboVS, sizeof(m_UboVS));

    }
}