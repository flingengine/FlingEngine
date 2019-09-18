#pragma once

#include "FlingVulkan.h"

namespace Fling
{
    /**
     * @brief A logical device represents the application view of the device
     */
    class LogicalDevice
    {
    public:

        explicit LogicalDevice(class Instance* t_Instance, class PhysicalDevice* t_PhysDevice, const VkSurfaceKHR t_Surface);

        ~LogicalDevice();

        const VkDevice& GetVkDevice() const { return m_Device; }

		const VkQueue& GetGraphicsQueue() const { return m_GraphicsQueue; }
		const VkQueue& GetPresentQueue() const { return m_PresentQueue; }

		const VkQueueFlags& GetSupportedQueues() const { return m_SupportedQueues; }

		UINT32 GetGraphicsFamily() const { return m_GraphicsFamily; }
		UINT32 GetPresentFamily() const { return m_PresentFamily; }

		void WaitForIdle();


    private:

        /** The vulkan logical device */
        VkDevice m_Device = VK_NULL_HANDLE;

        const class Instance* m_Instance;
		const class PhysicalDevice* m_PhysicalDevice;
		const VkSurfaceKHR m_Surface;
        
        /** Handle for the graphics queue */
        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;

        /** Handle to the presentation queue */
        VkQueue m_PresentQueue = VK_NULL_HANDLE;

		/** Queue families */
		VkQueueFlags m_SupportedQueues{};
		UINT32 m_GraphicsFamily = 0;
		UINT32 m_PresentFamily = 0;
		UINT32 m_ComputeFamily = 0;
		UINT32 m_TransferFamily = 0;

		/**
		 * @brief	Get what queue Indecies/families this device should use
		 */
		void CreateQueueIndecies();

        /**
         * @brief Create the Vk resoruces for this logical device
         */
        void CreateDevice();
    };
}   // namespace Fling