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

	/**
	* @brief	Core rendering functionality of the Fling Engine. Controls what Render pipelines 
	*			are available
	*/
    class VulkanApp : public Singleton<VulkanApp>
    {
    public:

		void Init(PipelineFlags t_Conf, entt::registry& t_Reg);
		void Shutdown() override;

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

    private:
		
		/**
		* @brief	Prepare logical, physical and swap chain devices. 
		*			Prepares window based on the Fling Config
		*/
		void Prepare();

		/**
		* @brief	Creates a window and preps the VkSurfaceKHR 
		*/
		void CreateGameWindow(const UINT32 t_width, const UINT32 t_height);

		VkExtent2D ChooseSwapExtent();

		void BuildRenderPipelines(PipelineFlags t_Conf, entt::registry& t_Reg);

		Instance* m_Instance = nullptr;
		LogicalDevice* m_LogicalDevice = nullptr;
		PhysicalDevice* m_PhysicalDevice = nullptr;
		Swapchain* m_SwapChain = nullptr;
		FlingWindow* m_CurrentWindow = nullptr;

		/** Handle to the surface extension used to interact with the windows system */
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		
		// Command Buffer pool
		VkCommandPool m_CommandPool = VK_NULL_HANDLE;

		std::vector<RenderPipeline*> m_RenderPipelines;

		// VMA Allocator
    };
}   // namespace Fling