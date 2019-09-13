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

        explicit LogicalDevice(class Instance* t_Instance, class PhysicalDevice* t_PhysicalDevice);

        ~LogicalDevice();


        const VkDevice& GetVkDevice() const { return m_Device; }

        /**
         * @brief Prepare for shutdown of this device
         */
        void PrepShutdown();

    private:

        /** The vulkan logical device  */
        VkDevice m_Device = VK_NULL_HANDLE;

        const class Instance* m_Instance;

        const class PhysicalDevice* m_PhysDevice;

        /** Handle for the graphics queue */
        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;

        /** Handle to the presentation queue */
        VkQueue m_PresentQueue = VK_NULL_HANDLE;

        /** Device extension support for the swap chain */
        const std::vector<const char*> m_DeviceExtensions = 
        {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        /**
         * @brief Create the Vk resoruces for this logical device
         */
        void CreateDevice();
    };
}   // namespace Fling