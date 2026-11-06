#pragma once

#include "FlingTypes.h"
#include "FlingVulkan.h"
#include "Platform.h"

#include <vector>

namespace Fling
{
    /**
     * The instance is a representation of this application graphics instance in Vulkan
     */
    class Instance
    {
    public:
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;

        explicit Instance();

        ~Instance();

        const VkInstance& GetRawVkInstance() const { return m_Instance; }

        bool IsValidationEnabled() const { return m_EnableValidationLayers; }

        uint32 EnabledValidationLayerCount() const { return static_cast<uint32>(m_ValidationLayers.size()); }

        const std::vector<const char*>& GetEnabledValidationLayers() const { return m_ValidationLayers; }

        const std::vector<const char*>& GetEnabledExtensions() const { return m_DeviceExtensions; };

        /**
         * Return the "most capable" available physical device we should use.
         * We prefer discrete GPUs over most other options.
         */
        VkPhysicalDevice GetBestPhysicalDevice() const;

    private:
        /** The Vulkan instance */
        VkInstance m_Instance = VK_NULL_HANDLE;

        /**
         * If this instance has validation layers enabled. This is read from the config file.
         * Default to false if no config
         */
        uint8 m_EnableValidationLayers : 1;

        /**
         * Create the VkInstance of this object and application information
         */
        void CreateInstance();

        std::vector<const char*> GetRequiredExtensions();

        bool CheckValidationLayerSupport();

        /**
         * All available physical VK devices on this machine.
         */
        std::vector<VkPhysicalDevice> GetAllAvailableDevices() const;

        /**
         * Choose the best of the given physical devices, favoring discrete GPUs and
         * requiring support for all of the given extensions.
         *
         * @return The best device, or VK_NULL_HANDLE if none are suitable
         */
        static VkPhysicalDevice ChooseBestPhysicalDevice(const std::vector<VkPhysicalDevice>& t_AvailableDevices,
                                                         const std::vector<const char*>& t_EnabledExtensions);

#if FLING_DEBUG
        // Debug messenger callbacks ---------------------------
        /** Debug message handler for Vulkan */
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;

        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT t_messageSeverity,
                                                            VkDebugUtilsMessageTypeFlagsEXT t_messageType,
                                                            const VkDebugUtilsMessengerCallbackDataEXT* t_CallbackData,
                                                            void* t_UserData);

        VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                              const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                              const VkAllocationCallbacks* pAllocator,
                                              VkDebugUtilsMessengerEXT* pDebugMessenger);

        void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                           VkDebugUtilsMessengerEXT debugMessenger,
                                           const VkAllocationCallbacks* pAllocator);

        /**
         * Create a debug message callback from vulkan and allow us to log errors from it
         * @see Instance::DebugCallback
         */
        void SetupDebugMessages();
#endif

        /** The validation layers that we want to look for on this instance */
        const std::vector<const char*> m_ValidationLayers = {"VK_LAYER_LUNARG_standard_validation"};

        /** Device extension support for the swap chain */
        const std::vector<const char*> m_DeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    };
} // namespace Fling