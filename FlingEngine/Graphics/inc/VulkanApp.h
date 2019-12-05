#pragma once

#include "FlingTypes.h"
#include "FlingVulkan.h"
#include "Singleton.hpp"

#include <entt/entity/registry.hpp>
#include <vector>

namespace Fling
{
	/** Configuration that can determine what render pipelines will be added to this application */
	enum PipelineFlags
	{
		DEFERRED = 0x01,
		REFLECTIONS = 0x02,
		IMGUI = 0x04,	
		ALL = 0xff
	};

	class Instance;
	class LogicalDevice;
	class PhysicalDevice;
	class Swapchain;
	class FlingWindow;
	class RenderPipeline;
	class CommandBuffer;
	class FirstPersonCamera;
	class DepthBuffer;

	/**
	* @brief	Core rendering functionality of the Fling Engine. Controls what Render pipelines 
	*			are available
	*/
    class VulkanApp : public Singleton<VulkanApp>
    {
    public:

		/** 
		* Having multiple frames in flight allows the GPU to start processing the next frame 
		* while the current one is being drawn on the screen
		*/
		static const INT32 MAX_FRAMES_IN_FLIGHT;

		void Init(PipelineFlags t_Conf, entt::registry& t_Reg);
		void Shutdown() override;

		/** 
		* Specify default constructible so that we have more explicit 
		* control with Init and Shutdown 
		*/
        VulkanApp() = default;
        ~VulkanApp() = default;

		/**
		* @brief	Updates all rendering buffers and sends commands to draw a frame
		*/
		void Update(float DeltaTime, entt::registry& t_Reg);

		inline FlingWindow* GetCurrentWindow() const { return m_CurrentWindow; }
		inline LogicalDevice* GetLogicalDevice() const { return m_LogicalDevice; }
		inline PhysicalDevice* GetPhysicalDevice() const { return m_PhysicalDevice; }
		inline const VkCommandPool GetCommandPool() const { return m_CommandPool; }
		inline VkRenderPass GetGlobalRenderPass() const { return m_RenderPass; }
		inline VkFramebuffer GetFrameBufferAtIndex(UINT32 index) const { return m_SwapChainFrameBuffers[index]; }

		/**
		* @brief	Request a new command buffer from the command buffer pool
		*/
		CommandBuffer* RequestCommandBuffer();

    private:
		
		/**
		* @brief	Prepare logical, physical and swap chain devices. 
		*			Prepares window based on the Fling Config
		*/
		void Prepare();

		/**
		* @breif	Create semaphores for available swap chain images and fences 
		*			for the current frame in flight
		*/
		void CreateFrameSyncResources();

		/**
		* @brief	Creates a window and preps the VkSurfaceKHR 
		*/
		void CreateGameWindow(const UINT32 t_width, const UINT32 t_height);

		/** Returns the current extents needed to render based on the physical device and surface */
		VkExtent2D ChooseSwapExtent();

		/** Builds any render pipelines with their specific set of sub passes and shaders */
		void BuildRenderPipelines(PipelineFlags t_Conf, entt::registry& t_Reg);

		/**
		 * @brief	Build the frame buffers for each swap chain image along with the render pass
		 *			for it to use
		 */
		void BuildSwapChainResources();

		void BuildGlobalRenderPass();

		void BuildSwapChainFrameBuffer();

		/** Vulkan Devices that need to get created. @See VulkanApp::Prepare */
		Instance* m_Instance = nullptr;
		LogicalDevice* m_LogicalDevice = nullptr;
		PhysicalDevice* m_PhysicalDevice = nullptr;
		FlingWindow* m_CurrentWindow = nullptr;
		
		// Swap chain related stuff ---------------------------------------------------------------------
		Swapchain* m_SwapChain = nullptr;
		// Depth buffer acts as a container for our swap chain depth attachment
		DepthBuffer* m_DepthBuffer = nullptr;
		// Global render pass for frame buffer usage
		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
		// List of available frame buffers (same as number of swap chain images)
		std::vector<VkFramebuffer> m_SwapChainFrameBuffers;

		// Stages that the swap chain needs to wait on in order to present
		VkPipelineStageFlags m_WaitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		/** Keep a vector of command buffers that we want to use so that we can have one for each active frame */
		std::vector<CommandBuffer*> m_DrawCmdBuffers;

		/** Synchronization primitives for drawing the frame. @see VulkanApp::CreateFrameSyncResources */
		std::vector<VkSemaphore> m_ImageAvailableSemaphores;
		std::vector<VkSemaphore> m_RenderFinishedSemaphores;
		std::vector<VkFence> m_InFlightFences;

		/** Handle to the surface extension used to interact with the windows system */
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		
		size_t CurrentFrameIndex = 0;

		// Command Buffer pool
		VkCommandPool m_CommandPool = VK_NULL_HANDLE;

		std::vector<RenderPipeline*> m_RenderPipelines;

		/** The Vulkan app will specify the current camera and be limited to one for now */
		FirstPersonCamera* m_Camera = nullptr;

		// #TODO VMA Allocator
    };
}   // namespace Fling