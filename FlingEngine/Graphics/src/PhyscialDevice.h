#pragma once

#include "FlingTypes.h"
#include "FlingVulkan.h"
#include "Strings/String.h"

#include <string>
#include <vector>

namespace Fling
{
    /**
     * A physical device represents the Vulkan physical device (the GPU) that
     * we are currently using
     */
    class PhysicalDevice
    {
    public:
        explicit PhysicalDevice(VkPhysicalDevice t_Device);

        /** The physical device will get cleaned up when the Instance does automatically */
        ~PhysicalDevice() noexcept = default;

        operator const VkPhysicalDevice&() const { return m_PhysicalDevice; }
        const VkPhysicalDevice& GetVkPhysicalDevice() const { return m_PhysicalDevice; }

        const VkPhysicalDeviceProperties& GetDeviceProps() const { return m_DeviceProperties; }
        const VkPhysicalDeviceFeatures& GetDeivceFeatures() const { return m_DeviceFeatures; }

        /** Logs info about this physical device (vendor, model, ID, etc) to the console/Log file */
        void LogPhysicalDeviceInfo();

        /**
         * Checks hte given format properties that are supported on this physical device
         */
        VkFormatProperties GetFormatProperties(VkFormat t_Form) const;

        VkSampleCountFlagBits GetMaxUsableSampleCount();

        VkBool32 GetSupportedDepthFormat(VkFormat* depthFormat) const;

        /**
         * Formatted string of this device's type, ID, vendor, and API version.
         */
        String ToString() const;

    private:
        /** The Vulkan physical device */
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;

        /** Misc. Device properties that may be useful */
        VkPhysicalDeviceProperties m_DeviceProperties{};
        VkPhysicalDeviceFeatures m_DeviceFeatures{};
        VkPhysicalDeviceMemoryProperties m_MemoryProperties{};

        /** The max supported MSSA level on this device */
        VkSampleCountFlagBits m_MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    };
} // namespace Fling