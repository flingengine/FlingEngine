#pragma once

#include "Renderer.h"
#include "Model.h"
#include "Image.h"
#include "FlingVulkan.h"
#include "ResourceManager.h"
#include "Buffer.h"
#include "FlingTypes.h"

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
                std::shared_ptr<class Image> t_NegZ);

            ~Cubemap();

            void PreparePipeline();
            void LoadCubemap();

            /**
             * @brief Get the Uniform Buffer object
             * 
             * @return std::unique_ptr<Buffer> 
             */
            std::shared_ptr<Buffer> GetUniformBuffer() { return m_uniformBuffer; }

            /**
             * @brief Get the Pipeline Layout object
             * 
             * @return const VkPipelineLayout 
             */
            const VkPipelineLayout GetPipelineLayout() { return m_pipelineLayout; }

            /**
             * @brief Get the Pipe Line object
             * 
             * @return const VkPipline 
             */
            const VkPipeline GetPipeLine() { return m_pipeline; }

            /**
             * @brief Get the Descriptor Sets object
             * 
             * @return const std::vector<VkDescriptorSet> 
             */
            const std::vector<VkDescriptorSet> GetDescriptorSets() { return m_descriptorSets; }
            
            /**
             * @brief Get the Vertex Buffer object
             * 
             * @return const Buffer* 
             */
            const Buffer* GetVertexBuffer() { return m_cube->GetVertexBuffer(); }

            /**
             * @brief Get the Index Buffer object
             * 
             * @return const Buffer* 
             */
            const Buffer* GetIndexBuffer() { return m_cube->GetIndexBuffer(); }

            /**
             * @brief Get the Index Count object
             * 
             * @return const UINT32 
             */
            const UINT32 GetIndexCount() { return m_cube->GetIndexCount(); }

        private:
            std::vector<Image> m_cubeMapImages;
            std::vector<VkDescriptorSet> m_descriptorSets;

            VkDeviceMemory m_uniformBufferMemory;
            VkImage m_cubeMapImage;
            VkDeviceMemory m_cubeMapImageMemory;
            VkSampler m_cubeMapSampler;
            VkDescriptorSetLayout m_descriptorSetLayout;
            VkDescriptorPool m_descriptorPool;
            VkPipelineLayout m_pipelineLayout;
            VkPipeline m_pipeline;

            std::shared_ptr<Model> m_cube;
            std::shared_ptr<Buffer> m_uniformBuffer;
    };
}