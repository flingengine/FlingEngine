#pragma once

#include "FlingVulkan.h"

namespace Fling
{
    /**
     * @brief A physical device represents the Vulkan physical device (the GPU) that 
     * we are currently using 
     */
    class PhysicalDevice
    {
    public:

        explicit PhysicalDevice(class Instance* t_Instance);
        
		/** The physical device will get cleaned up when the Instance does automatically */
        ~PhysicalDevice() noexcept = default;

        operator const VkPhysicalDevice &() const { return m_PhysicalDevice; }
		const VkPhysicalDevice& GetVkPhysicalDevice() const { return m_PhysicalDevice; }

		const VkPhysicalDeviceProperties& GetDeviceProps() const { return m_DeviceProperties; }
        const VkPhysicalDeviceFeatures& GetDeivceFeatures() const { return m_DeviceFeatures; } 

        /**
         * @brief Get a string representing the device vendor
         * 
         * @param t_Props 
         * @return const char* 
         */
        static const char* GetDeviceType(VkPhysicalDeviceProperties t_Props);

        static const char* GetDeviceVendor(VkPhysicalDeviceProperties t_Props);

		/** Logs info about this physical device (vendor, model, ID, etc) to the console/Log file */
		void LogPhysicalDeviceInfo();

    private:

		/**
		 * Choose the best available physical device on this machine favoring discrete GPU's and
		 * those who match all instance extensions
		 */
		VkPhysicalDevice ChooseBestPhyscialDevice(std::vector<VkPhysicalDevice>& t_AvailableDevices);

        /** The Vulkan physical device */
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;

		/** Owning instance of this physical device */
        const class Instance* m_Instance;

		/** Misc. Device properties that may be useful */
        VkPhysicalDeviceProperties m_DeviceProperties{};
        VkPhysicalDeviceFeatures m_DeviceFeatures{};
		VkPhysicalDeviceMemoryProperties m_MemoryProperties{};
    };
}   // namespace Fling