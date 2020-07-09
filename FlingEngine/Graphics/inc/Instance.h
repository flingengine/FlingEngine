#pragma once

#include "FlingVulkan.h"
#include "NonCopyable.hpp"

namespace Fling
{
    /**
     * @brief The instance is a representation of this application graphics instance in Vulkan
     */
    class Instance : NonCopyable
    {
    public:

        explicit Instance();

        ~Instance();

        const VkInstance& GetRawVkInstance() const { return m_Instance; }

        bool IsValidationEnabled() const { return m_EnableValidationLayers; } 

        uint32 EnabledValidationLayerCount() const { return static_cast<uint32>(m_ValidationLayers.size()); }

		const std::vector<const char*>& GetEnabledValidationLayers() const { return m_ValidationLayers; }

		const std::vector<const char*>& GetEnabledExtensions() const { return m_DeviceExtensions; };

    private:

        /** The Vulkan instance */
        VkInstance m_Instance = VK_NULL_HANDLE;

        /**
         * If this instance has validation layers enabled. This is read from the config file. 
         * Default to false if no config 
         */
        uint8 m_EnableValidationLayers : 1;

        /**
         * @brief Create the VkInstance of this object and application information
         */
        void CreateInstance();

        std::vector<const char*> GetRequiredExtensions();

        bool CheckValidationLayerSupport();

#if FLING_DEBUG
        // Debug messenger callbacks ---------------------------
        /** Debug message handler for Vulkan */
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
        
        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT t_messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT t_messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* t_CallbackData,
            void* t_UserData
        );

        VkResult CreateDebugUtilsMessengerEXT(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
            const VkAllocationCallbacks* pAllocator,
            VkDebugUtilsMessengerEXT* pDebugMessenger 
        );

        void DestroyDebugUtilsMessengerEXT(
            VkInstance instance,
            VkDebugUtilsMessengerEXT debugMessenger,
            const VkAllocationCallbacks* pAllocator 
        );

		/**
		 * Create a debug message callback from vulkan and allow us to log errors from it
		 * @see Instance::DebugCallback
		 */
        void SetupDebugMessages();
#endif

        /** The validation layers that we want to look for on this instance */
        const std::vector<const char*> m_ValidationLayers =
        {
            "VK_LAYER_LUNARG_standard_validation"
        };

		/** Device extension support for the swap chain */
		const std::vector<const char*> m_DeviceExtensions =
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

    };
}   // namespace Fling