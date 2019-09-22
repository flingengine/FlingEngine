#pragma once

// Resolve warnings
#include "Platform.h"

#include "FlingVulkan.h"

#include <GLFW/glfw3.h>

#include "Singleton.hpp"

#include "Vertex.h"
#include "UniformBufferObject.h"
#include "FirstPersonCamera.h"

#include "FlingWindow.h"
#include "PhyscialDevice.h"
#include "Instance.h"
#include "LogicalDevice.h"
#include "Buffer.h"
#include "SwapChain.h"
#include "DepthBuffer.h"
#include "Model.h"

namespace Fling
{
    // File resource
    class File;

    /// <summary>
    /// Core renderer for the application
    /// </summary>
    class Renderer : public Singleton<Renderer>
    {
    public:

        virtual void Init() override;

        virtual void Shutdown() override;

        /// <summary>
        /// Init GLFW and create the game window
        /// </summary>
        /// <param name="t_width">Width of the window</param>
        /// <param name="t_height">Height of the window</param>
        void CreateGameWindow( const UINT32 t_width, const UINT32 t_height );

		FlingWindow* GetCurrentWindow() const { return m_CurrentWindow; }

		/** Happens before draw frame. Update the window  */
		void Tick();

        /**
        * Draw the frame!
        */
        void DrawFrame();

        /**
        * Prepare for shutdown of the rendering pipeline, close any open semaphores
        */
        void PrepShutdown();

        /**
         * @brief Get the logical graphics Device object
         * 
         * @return const ref to VkDevice
         */
        const VkDevice& GetLogicalVkDevice() const { return m_LogicalDevice->GetVkDevice(); }

		LogicalDevice* GetLogicalDevice() const { return m_LogicalDevice; }

        /**
         * @brief Get the Physical Device object used by this renderer
         * 
         * @return const VkPhysicalDevice& 
         */
        const VkPhysicalDevice& GetPhysicalVkDevice() const { return m_PhysicalDevice->GetVkPhysicalDevice(); }

		PhysicalDevice* GetPhysicalDevice() const { return m_PhysicalDevice; }

		const VkCommandPool& GetCommandPool() const { return m_CommandPool; }

        const VkQueue& GetGraphicsQueue() const { return m_LogicalDevice->GetGraphicsQueue(); }

        void SetFrameBufferHasBeenResized(bool t_Setting){ m_FrameBufferResized = t_Setting; }

		const VkSurfaceKHR& GetVkSurface() const { return m_Surface; }

		Swapchain* GetSwapChain() const { return m_SwapChain; }

    private:

        /// <summary>
        /// Init the current graphics API
        /// </summary>
        void InitGraphics();

        /**
         * @brief Create a Descriptor Layout object
         * @see UniformBufferObject.h
         */
        void CreateDescriptorLayout();

        /**
        * Create the graphics pipeline (IA, VS, FS, etc)
        */
        void CreateGraphicsPipeline();

        /**
        * Create the frame buffer that will be used by the graphics pipeline
        */
        void CreateRenderPass();

        /**
        * Create the frame buffers for use by the swap chain
        */
        void CreateFrameBuffers();

        /**
        * Create the command pool to be sent every frame
        */
        void CreateCommandPool();

        void CreateCommandBuffers();

        /**
        * Create semaphores and fence objects
        */
        void CreateSyncObjects();

        void CleanupFrameResources();

        /**
        * Re-create the image views, render passes, and command buffers
        */
        void RecreateFrameResources();

        void CreateUniformBuffers();

        void CreateDescriptorPool();

        void CreateDescriptorSets();

        /**
        * Check the swap chain support of a given device
        * @param 	The device to check support on
        *
        * @return   Details of the the swap chain support on this device
        */
        SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice t_Device);

        /**
        * Choose a swap chain format based on the available formats. Prefer to format
        * that has VK_FORMAT_B8G8R8A8_UNORM and VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, otherwise
        * get the first available. 
        * 
        * @param    Available swap chain formats
        *
        * @return   Best swap chain surface formate based on the available ones
        */
        VkSurfaceFormatKHR ChooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& t_AvailableFormats);

        /**
        * Choose a present mode for the swap chain based on the given formats. Prefer VK_PRESENT_MODE_MAILBOX_KHR
        * If none are available, than return VK_PRESENT_MODE_FIFO_KHR or VK_PRESENT_MODE_IMMEDIATE_KHR based on support
        * 
        * @param    Vector of available formats
        *
        * @return   Preferred present mode from the available formats   	
        */
        VkPresentModeKHR ChooseSwapChainPresentMode(const std::vector< VkPresentModeKHR>& t_AvialableFormats);

        /**
        * Determine the best match extents based on our window width and height
		*
        * @return   Extents with the best matching resolution
        */
        VkExtent2D ChooseSwapExtent();

        /**
         * @brief Update the uniform buffer data. Called during DrawFrame
         * 
         * @param t_CurrentImage The current image index that we are using
         */
        void UpdateUniformBuffer(UINT32 t_CurrentImage);

        /**
        * Create a shader module based on the given shader code
        * @param 	Vector of the shader code
        *
        * @return   Shader module from the given code
        */
        VkShaderModule CreateShaderModule(std::shared_ptr<File> t_ShaderCode);

		/** Camera Instance */
		std::unique_ptr<FirstPersonCamera> m_camera;

		FlingWindow* m_CurrentWindow = nullptr;

        Instance* m_Instance = nullptr;

        LogicalDevice* m_LogicalDevice = nullptr;

        PhysicalDevice* m_PhysicalDevice = nullptr;

        /** Handle to the surface extension used to interact with the windows system */
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

		Swapchain* m_SwapChain = nullptr;

        VkRenderPass m_RenderPass;

        /** Pipeline layout stores uniforms (global shader vars) */
        VkDescriptorSetLayout m_DescriptorSetLayout;
        VkPipelineLayout m_PipelineLayout;

        VkPipeline m_GraphicsPipeline;

        /** @see Renderer::CreateCommandPool */
        VkCommandPool m_CommandPool;

        /** @see CreateDescriptorPool */
        VkDescriptorPool m_DescriptorPool;

		DepthBuffer* m_DepthBuffer = nullptr;

        size_t CurrentFrameIndex = 0;

        /** Used to determine if the frame buffer has been resized or not */
        bool m_FrameBufferResized = false;

        static const int MAX_FRAMES_IN_FLIGHT;

        /** Uniform buffers */
        std::vector<Buffer*> m_UniformBuffers;
        
        std::vector<VkDescriptorSet> m_DescriptorSets;

        /** 
        * The frame buffers for the swap chain 
        * @see Renderer::CreateFrameBuffers 
        */
        std::vector<VkFramebuffer> m_SwapChainFramebuffers;

        /**
        * Command buffers 
        * @see m_CommandPool
        */
        std::vector<VkCommandBuffer> m_CommandBuffers;

        std::vector<VkSemaphore> m_ImageAvailableSemaphores;
        std::vector<VkSemaphore> m_RenderFinishedSemaphores;
        std::vector<VkFence> m_InFlightFences;

		std::shared_ptr<class Image> m_TestImage;

		std::shared_ptr<Model> m_TestModel;
    };
}	// namespace Fling