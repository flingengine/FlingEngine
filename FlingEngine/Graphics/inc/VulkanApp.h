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
		DEFERRED 		= (1u << 0),
		REFLECTIONS 	= (1u << 1),
		IMGUI 			= (1u << 2),
		CUBEMAP 		= (1u << 3),
		DEBUG			= (1u << 4),
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
	class BaseEditor;

	/**
	* @brief	Core rendering functionality of the Fling Engine. Controls what Render pipelines 
	*			are available
	*/
    class VulkanApp : public Singleton<VulkanApp>
    {
    public:

		void Init(PipelineFlags t_Conf, entt::registry& t_Reg, std::shared_ptr<Fling::BaseEditor> t_Editor);
		void Shutdown(entt::registry& t_Reg);

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
		inline FirstPersonCamera* GetCamera() const { return m_Camera; }

	protected:
		void Init() override {}
		void Shutdown() override {}

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
		void BuildRenderPipelines(PipelineFlags t_Conf, entt::registry& t_Reg, std::shared_ptr<Fling::BaseEditor> t_Editor);

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
		/** The clear values that will be used when building the command buffer to run this subpass */
		std::vector<VkClearValue> m_SwapChainClearVals = std::vector<VkClearValue>(2);

		// Stages that the swap chain needs to wait on in order to present
		VkPipelineStageFlags m_WaitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		/** Keep a vector of command buffers that we want to use so that we can have one for each active frame */
		std::vector<CommandBuffer*> m_DrawCmdBuffers;

		/** Synchronization primitives for drawing the frame. @see VulkanApp::CreateFrameSyncResources */
		std::vector<VkSemaphore> m_PresentCompleteSemaphores;
		std::vector<VkSemaphore> m_RenderFinishedSemaphores;
		std::vector<VkFence> m_InFlightFences;

		/** Handle to the surface extension used to interact with the windows system */
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		
		/** The index of the current frame in flight. This is controlled with the in flight fences 
		and is updated every Update of the vulkan app. */
		size_t CurrentFrameIndex = 0;

		// Command Buffer pool
		VkCommandPool m_CommandPool = VK_NULL_HANDLE;

		std::vector<RenderPipeline*> m_RenderPipelines;

		/** The Vulkan app will specify the current camera and be limited to one for now */
		FirstPersonCamera* m_Camera = nullptr;

		// #TODO VMA Allocator
    };
}   // namespace Fling