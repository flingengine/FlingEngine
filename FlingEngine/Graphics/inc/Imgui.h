#pragma once 

#include "FlingVulkan.h"
#include "Renderer.h"
#include "Buffer.h"
#include "LogicalDevice.h"


namespace Fling 
{
    class ImGUI 
    {
        public:
            ImGUI();
            ~ImGUI();

            void Init(float width, float height);
            void InitResources(VkRenderPass renderPass, VkQueue copyQueue);
            //To do : look into delegates 
            void NewFrame();
            void UpdateBuffers();
            void DrawFrame();
            
        private:
            VkSampler m_sampler;
            Buffer m_vertexBuffer;
            Buffer m_indexBuffer;            
            INT32 m_vertexCount;
            INT32 m_indexCount;
            VkDeviceMemory m_fontMemory = VK_NULL_HANDLE;
            VkImage m_fontImage = VK_NULL_HANDLE;
            vkImageView m_fontView = VK_NULL_HANDLE;
            VkPipeLineCache m_pipelineCache;
            VkPipeLineLayout m_pipeLineLayout;
            VkPipeLine m_pipeLine;
            VkDescriptorPool m_descriptorPool;
            VkDescriptorSetLayout m_descriptorSetLayout;
            VkDescriptorSet m_descriptorSet;
            LogicalDevice* m_device;
            Renderer& m_renderer;
    }
}