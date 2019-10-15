#pragma once

#include "Renderer.h"
#include "Model.h"
#include "Image.h"
#include "FlingVulkan.h"
#include "ResourceManager.h"
#include "Buffer.h"
#include "FlingTypes.h"
#include "GraphicsHelpers.h"
#include "FlingMath.h"
#include "Vertex.h"
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
                std::shared_ptr<class Image> t_PosX,
                std::shared_ptr<class Image> t_NegX,
                std::shared_ptr<class Image> t_PosY,
                std::shared_ptr<class Image> t_NegY,
                std::shared_ptr<class Image> t_PosZ,
                std::shared_ptr<class Image> t_NegZ,
                VkRenderPass t_renderPass,
                VkDevice t_logicalDevice);

            ~Cubemap();

            void Init(std::shared_ptr<Camera> t_camera);

            void UpdateUniformBuffer(const glm::mat4& t_projectionMatrix, glm::vec3 t_rotation);

            /**
             * @brief Get the Uniform Buffer object
             * 
             * @return std::unique_ptr<Buffer> 
             */
            std::shared_ptr<Buffer> GetUniformBuffer() const { return m_UniformBuffer; }

            /**
             * @brief Get the Pipeline Layout object
             * 
             * @return const VkPipelineLayout 
             */
            const VkPipelineLayout& GetPipelineLayout() const { return m_PipelineLayout; }

            /**
             * @brief Get the Pipe Line object
             * 
             * @return const VkPipline 
             */
            const VkPipeline& GetPipeLine() const { return m_Pipeline; }

            /**
             * @brief Get the Descriptor Sets object
             * 
             * @return const VkDescriptorSet> 
             */
            const VkDescriptorSet& GetDescriptorSet() const { return m_DescriptorSet; }

            /**
             * @brief Get the Vertex Buffer object
             * 
             * @return const Buffer* 
             */
            const Buffer* GetVertexBuffer() const { return m_Cube->GetVertexBuffer(); }

            /**
             * @brief Get the Index Buffer object
             * 
             * @return const Buffer* 
             */
            const Buffer* GetIndexBuffer() const { return m_Cube->GetIndexBuffer(); }

            /**
             * @brief Get the Index Count object
             * 
             * @return const UINT32 
             */
            const UINT32 GetIndexCount() const { return m_Cube->GetIndexCount(); }

        private:
            void PreparePipeline();
            void LoadCubemap();
            void SetupDescriptors();

            std::vector<Image> m_cubeMapImages;

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

            std::shared_ptr<Model> m_Cube;
            std::shared_ptr<class Buffer> m_UniformBuffer;

            UboSkyboxVS m_UboVS;
    };
}