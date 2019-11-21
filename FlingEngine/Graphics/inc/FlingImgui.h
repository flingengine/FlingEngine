#pragma once 

#if WITH_IMGUI
#include <imgui.h>


#include <algorithm>

#include "Renderer.h"
#include "FlingVulkan.h"
#include "LogicalDevice.h"
#include "GraphicsHelpers.h"
#include "ResourceManager.h"
#include "File.h"
#include "SwapChain.h"


namespace Fling 
{
    class FlingImgui
    {
    public:
        // UI params are set via push constants
        struct PushConstBlock {
            glm::vec2 scale;
            glm::vec2 translate;
        } pushConstBlock;

        FlingImgui(LogicalDevice* t_logicalDevice, Swapchain* t_swapChain);
        ~FlingImgui();
        
        /**
         * @brief Sets imgui style and intializes imgui related buffers and pools 
         * 
         * @param t_width 
         * @param t_height 
         */
        void Init(float t_width, float t_height);

        /**
         * @brief Destroys imgui resources
         * 
         */
        void Release();
        
        /**
         * @brief Creates imgui graphics pipeline 
         * 
         * @param t_copyQueue 
         */
        void InitResources(VkQueue t_copyQueue);
        
        /**
         * @brief 
         * 
         * @tparam Candidate: type of the function delegate
         * @tparam Type: object type 
         * @param t_instance: object instance 
         */
        template <auto Candidate, typename Type>
        void SetDisplay(Type& t_instance)
        {
            m_DisplayCallback.connect<Candidate>(t_instance);
        }

        /**
         * @brief Start a new ImGUI Frame and prepare for command buffers to be built. 
         * 
         */
        void PrepFrameBuild();

        /**
         * @brief Command buffers start recoding current renderpass with updated index and vertex buffers
         *
         * @param t_displayOn
         */
        void BuildCommandBuffers(bool t_displayOn);
        
        const VkCommandBuffer GetCommandBuffer(UINT32 t_index) const { return m_commandBuffers[t_index];  }
        const VkCommandPool GetCommandPool() const { return m_commandPool; }

    private:
        /**
         * @brief Updates the vertex and index buffers for imgui 
         * 
         */
        void UpdateBuffers();
        /**
         * @brief Submits imgui render commands to its command buffer
         * 
         * @param t_commandBuffer current command buffer
         */
        void DrawFrame(VkCommandBuffer t_commandBuffer);

        VkSampler m_sampler;
        INT32 m_vertexCount = 0;
        INT32 m_indexCount = 0;
        VkDeviceMemory m_fontMemory = VK_NULL_HANDLE;
        VkImage m_fontImage = VK_NULL_HANDLE;
        VkImageView m_fontImageView = VK_NULL_HANDLE;
        VkPipelineCache m_pipelineCache;
        VkPipelineLayout m_pipelineLayout;
        VkPipeline m_pipeLine;
        VkDescriptorPool m_descriptorPool;
        VkDescriptorSetLayout m_descriptorSetLayout;
        VkDescriptorSet m_descriptorSet;
        LogicalDevice* m_LogicalDevice;
        
        VkRenderPass m_renderPass;

        VkCommandPool m_commandPool;
        std::vector<VkCommandBuffer> m_commandBuffers;
        std::vector<VkFramebuffer> m_frameBuffers;

        std::unique_ptr<class Buffer> m_vertexBuffer;
        std::unique_ptr<class Buffer> m_indexBuffer;

        Swapchain* m_swapChain;

        entt::delegate<void()> m_DisplayCallback{};
    };
} //namespace fling

#endif // WITH_IMGUI