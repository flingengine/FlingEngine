#pragma once

#include "Model.h"
#include "Image.h"
#include "FlingVulkan.h"
#include "Buffer.h"
#include "FlingTypes.h"
#include "FlingMath.h"
#include "File.h"
#include "LogicalDevice.h"
#include "UniformBufferObject.h"
#include "Camera.h"

namespace Fling
{
    class Cubemap
    {
        public:
            Cubemap(
                Guid t_PosX_ID,
                Guid t_NegX_ID,
                Guid t_PosY_ID,
                Guid t_NegY_ID,
                Guid t_PosZ_ID,
                Guid t_NegZ_ID,
                Guid t_VertexShader,
                Guid t_FragShader,
                VkRenderPass t_RenderPass,
                VkDevice t_LogicalDevice);

            ~Cubemap();

            void Init(Camera* t_Camera, UINT32 t_CurrentImage, size_t t_NumeFramesInFlight);

            void UpdateUniformBuffer(UINT32 t_CurrentImage, const glm::mat4& t_ProjectionMatrix, const glm::mat4& t_ViewMatrix);

            /**
             * @brief Get the Pipeline Layout object
             * 
             * @return const VkPipelineLayout 
             */
            VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

            /**
             * @brief Get the Pipe Line object
             * 
             * @return const VkPipline 
             */
            VkPipeline GetPipeLine() const { return m_Pipeline; }

            /**
             * @brief Get the Descriptor Sets object
             * 
             * @return const VkDescriptorSet> 
             */
            VkDescriptorSet& GetDescriptorSet() { return m_DescriptorSet; }

            /**
             * @brief Get the Vertex Buffer object
             * 
             * @return const Buffer* 
             */
            Buffer* GetVertexBuffer() const { return m_Cube->GetVertexBuffer(); }

            /**
             * @brief Get the Index Buffer object
             * 
             * @return const Buffer* 
             */
            Buffer* GetIndexBuffer() const { return m_Cube->GetIndexBuffer(); }

            /**
             * @brief Get the Index Count object
             * 
             * @return const UINT32 
             */
            UINT32 GetIndexCount() const { return m_Cube->GetIndexCount(); }

            VkIndexType GetIndexType() const { return m_Cube->GetIndexType(); }

        private:

            void PreparePipeline();
            
            void LoadCubemap(
                Guid t_PosX_ID,
                Guid t_NegX_ID,
                Guid t_PosY_ID,
                Guid t_NegY_ID,
                Guid t_PosZ_ID,
                Guid t_NegZ_ID);

            void SetupDescriptors();

            std::vector<Image> m_cubeMapImages;
            std::vector<std::shared_ptr<class Buffer>> m_UniformBuffers;

            VkImage m_Image;
            VkImageView m_Imageview;
            VkImageLayout m_ImageLayout;
            VkDeviceMemory m_ImageMemory;
            VkSampler m_Sampler;
            
            VkDescriptorSetLayout m_DescriptorSetLayout;
            VkDescriptorSet m_DescriptorSet;
            VkDescriptorPool m_DescriptorPool;
            VkPipelineLayout m_PipelineLayout;
            VkPipeline m_Pipeline;
            VkRenderPass m_RenderPass;
            VkDevice m_Device;
            VkPipelineCache m_PipelineCache;
            VkDescriptorBufferInfo m_UniformBufferDescriptor;

            VkDeviceSize m_ImageSize;
            VkDeviceSize m_LayerSize;
            VkFormat m_Format;
            UINT32 m_NumChannels;
            UINT32 m_MipLevels;
            size_t m_numsFrameInFlight;

            UboSkyboxVS m_UboVS;
            std::shared_ptr<Model> m_Cube;

            Guid m_FragShader;
            Guid m_VertexShader;
    };
}