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
#include "Instance.h"
#include "PhyscialDevice.h"
#include "LogicalDevice.h"
#include "Buffer.h"
#include "SwapChain.h"

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
        const VkDevice& GetDevice() const { return m_Device; }

        /**
         * @brief Get the Physical Device object used by this renderer
         * 
         * @return const VkPhysicalDevice& 
         */
        const VkPhysicalDevice& GetPhysicalDevice() const { return m_PhysicalDevice->GetVkPhysicalDevice(); } 

        const VkCommandPool& GetCommandPool() const { return m_CommandPool; }

        const VkQueue& GetGraphicsQueue() const { return m_GraphicsQueue; }

        void SetFrameBufferHasBeenResized(bool t_Setting){ m_FrameBufferResized = t_Setting; }

    private:

        /// <summary>
        /// Find what queue families are supported by a given 
        /// </summary>
        /// <param name="t_Device">Device to check</param>
        /// <returns>Queue family flags</returns>
        QueueFamilyIndices FindQueueFamilies( VkPhysicalDevice const t_Device );

        /// <summary>
        /// Init the current graphics API
        /// </summary>
        void InitGraphics();

        /// <summary>
        /// Create the logical vulkan device
        /// </summary>
        void CreateLogicalDevice();

        /// <summary>
        /// Configure graphics API debug callbacks [Vulkan]
        /// </summary>
        void SetupDebugMessesages();

        /**
        * Create the surface for Vulkan to use for integration with the window system
        * this surface can have an effect on the selection of physical device
        */
        void CreateSurface();

        /**
        * Create the swap chain and select the format, present mode, and extents
        */
        void CreateSwapChain();

        /**
        * Create the image views from the swap chain so that we can actually render them 
        */
        void CreateImageViews();

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
        * Create the frame buffer that will be used by the graphics piipeline
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


        void CleanUpSwapChain();

        /**
        * Re-create the image views, render passes, and command buffers
        */
        void RecreateSwapChain();

		/**
		* Create a vertex buffer using Temp_Vertices
		*/
        void CreateVertexBuffer();

		/**
		* Create an index buffer using Temp_Indecies
		*/
		void CreateIndexBuffer();

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
        * @param 	t_Capabilies    The available capabilities of the swap chain on this device
        *
        * @return   Extents with the best matching resolution
        */
        VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& t_Capabilies);

        /**
         * @brief Update the uniform buffer data. Called during DrawFrame
         * 
         * @param t_CurrentImage The current image index that we are using
         */
        void UpdateUniformBuffer(UINT32 t_CurrentImage);

        /**
        * Check if the given device supports the extensions that this application requires
        * 
        * @param t_Device       The device to check  		
        *
        * @return True if device supports our listed extensions
        * @see Renderer::m_DeviceExtensions
        */
        bool CheckDeviceExtensionSupport(VkPhysicalDevice t_Device);

        /**
        * Create a shader module based on the given shader code
        * @param 	Vector of the shader code
        *
        * @return   Shader module from the given code
        */
        VkShaderModule CreateShaderModule(std::shared_ptr<File> t_ShaderCode);

        void CreateTextureImage();

		/** Camera Instance */
		std::unique_ptr<FirstPersonCamera> m_camera;

		FlingWindow* m_CurrentWindow = nullptr;

        Instance* m_Instance = nullptr;

        LogicalDevice* m_LogicalDevice = nullptr;

        PhysicalDevice* m_PhysicalDevice = nullptr;

        /** Logical Vulkan device */
        VkDevice m_Device = VK_NULL_HANDLE;

        /** Handle for the graphics queue */
        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;

        /** Handle to the presentation queue */
        VkQueue m_PresentQueue = VK_NULL_HANDLE;

        /** Handle to the surface extension used to interact with the windows system */
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

        /** The swap chain of this renderer */
        VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;

        VkExtent2D m_SwapChainExtents;

        VkFormat m_SwapChainImageFormat;

        VkRenderPass m_RenderPass;

        /** Pipeline layout stores uniforms (global shader vars) */
        VkDescriptorSetLayout m_DescriptorSetLayout;
        VkPipelineLayout m_PipelineLayout;

        VkPipeline m_GraphicsPipeline;

        /** @see Renderer::CreateCommandPool */
        VkCommandPool m_CommandPool;

        /** @see CreateDescriptorPool */
        VkDescriptorPool m_DescriptorPool;

        /** Vertex and index buffers */
        Buffer* m_VertexBuffer = nullptr;
        Buffer* m_IndexBuffer = nullptr;

        size_t CurrentFrameIndex = 0;

        /** Used to determine if the frame buffer has been resized or not */
        bool m_FrameBufferResized = false;

        static const int MAX_FRAMES_IN_FLIGHT;

        /** Uniform buffers */
        std::vector<VkBuffer> m_UniformBuffers;
        std::vector<VkDeviceMemory> m_UniformBuffersMemory;
        
        std::vector<VkDescriptorSet> m_DescriptorSets;


        /** Device extension support for the swap chain */
        const std::vector<const char*> m_DeviceExtensions = 
        {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        /** The images inside of the swap chain */
        std::vector<VkImage> m_SwapChainImages;

        /** Image views from the swap chain */
        std::vector<VkImageView> m_SwapChainImageViews;

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
    };

    // Temp vectors of indecies/verts for testing while setting up the renderer

	const std::vector<UINT16> Temp_indices = 
	{
		0, 1, 2, 2, 3, 0
	};

	const std::vector<Vertex> Temp_Vertices = 
	{
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
	};

}	// namespace Fling