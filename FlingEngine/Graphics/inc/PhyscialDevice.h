#pragma once

#include "FlingVulkan.h"

namespace Fling
{
    /**
     * @brief Used to keep track of what properties a given queue satisfies
     */
    struct QueueFamilyIndices
    {
        UINT32 GraphicsFamily = 0;
        UINT32 PresentFamily = 0;

        /// <summary>
        /// Determines if this 
        /// </summary>
        /// <returns>True if queue family is complete</returns>
        bool IsComplete() const
        {
            return GraphicsFamily && PresentFamily;
        }
    };

    /**
     * @brief Represents the rating that this phsyical device has and is 
     * used to compare this device to other available ones.
     */
    struct PhysicalDeviceRating
    {
        UINT16 Score;
        VkPhysicalDeviceProperties DeviceProperties;
        VkPhysicalDeviceFeatures DeviceFeatures;
    };

    /**
     * @brief A physical device represents the vulkan physical device (the GPU) that 
     * we are currently using 
     */
    class PhysicalDevice
    {
    public:

        explicit PhysicalDevice(class Instance* t_Instance);
        
        ~PhysicalDevice();

        operator const VkPhysicalDevice &() const { return m_PhysicalDevice; }

        const VkPhysicalDevice& GetVkPhysicalDevice() const { return m_PhysicalDevice; }
        const VkPhysicalDeviceProperties& GetDeviceProps() const { return m_DeviceProperties; }
        const VkPhysicalDeviceFeatures& GetDeivceFeatures() const { return m_DeviceFeatures; } 

        UINT16 GetDeviceRating() const { return m_DeviceRating; }

    private:

        /**
         * @brief Get a rating of how good this device is for this application.
         * 
         * @param t_Device  Device to consider
         * @return UINT16   Score on a scale of 0 to 1000
         */
        static PhysicalDeviceRating GetDeviceRating( VkPhysicalDevice const t_Device );

        /** The vulkan physical device */
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;

        const class Instance* m_Instance;

        VkPhysicalDeviceProperties m_DeviceProperties;
        VkPhysicalDeviceFeatures m_DeviceFeatures;
        
        UINT16 m_DeviceRating = 0;

        void PickPhysicalDevice();

    };

}   // namespace Fling