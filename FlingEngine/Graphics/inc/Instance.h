#pragma once

#include "FlingVulkan.h"
#include "NonCopyable.hpp"

namespace Fling
{
    /**
     * @brief The instance is a represenation of this application graphics instance in Vulkan
     */
    class Instance : NonCopyable
    {
    public:

        Instance();

        ~Instance();

        const VkInstance& GetRawVkInstance() const { return m_Instance; }

        bool IsValidationEnabled() const { return m_EnableValidationLayers; } 

        UINT32 EnabledValidationLayerCount() const { return static_cast<UINT32>(m_ValidationLayers.size()); }

        const std::vector<const char*>& GetEnabledValidationLayers() const { return m_ValidationLayers; }

    private:

        /** The Vulkan instance */
        VkInstance m_Instance = VK_NULL_HANDLE;

        /** Debug message handler for Vulkan */
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;

        /**
         * If this instance has validation layers enabled. This is read from the config file. 
         * Default to false if no config 
         */
        UINT8 m_EnableValidationLayers : 1;

        /**
         * @brief Create the VkInstance of this object and application information
         */
        void CreateInstance();

        std::vector<const char*> GetRequiredExtensions();

        bool CheckValidationLayerSupport();


        // Debug messenger callbacks ---------------------------
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

        void SetupDebugMessages();

        /** The validation layers that we want to look for on this instance */
        const std::vector<const char*> m_ValidationLayers =
        {
            "VK_LAYER_KHRONOS_validation"
        };

    };
}   // namespace Fling