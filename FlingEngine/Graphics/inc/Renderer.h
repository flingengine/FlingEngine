#pragma once

// GLFW
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "Singleton.hpp"

namespace Fling
{
    /// <summary>
    /// Used to keep track of what properties a given queue satisfies
    /// </summary>
    struct QueueFamilyIndices
    {
        UINT8 GraphicsFamily : 1;

        /// <summary>
        /// Determines if this 
        /// </summary>
        /// <returns>True if queue family is complete</returns>
        bool IsComplete() const
        {
            return GraphicsFamily;
        }
    };

    /// <summary>
    /// Core renderer for the application
    /// </summary>
    class Renderer : public Singleton<Renderer>
    {
    public:

        virtual void Init() override;

        virtual void Shutdown() override;

        /// <summary>
        /// Init GLFW and create the game window
        /// </summary>
        /// <param name="t_width">Width of the window</param>
        /// <param name="t_height">Height of the window</param>
        void CreateGameWindow( const int t_width, const int t_height );

        /// <summary>
        /// Get a rating of how good this device is for this application.
        /// Scores range from 0 - 1000
        /// </summary>
        /// <param name="t_Device">Device to consider</param>
        /// <returns>Score on a scale of 0 to 1000</returns>
        static UINT16 GetDeviceRating( VkPhysicalDevice const t_Device );

        /// <summary>
        /// Find what queue families are supported by a given 
        /// </summary>
        /// <param name="t_Device">Device to check</param>
        /// <returns>Queue family flags</returns>
        static QueueFamilyIndices FindQueueFamilies( VkPhysicalDevice const t_Device );

    private:

        /// <summary>
        /// Init the current graphics API
        /// </summary>
        void InitGraphics();

        /// <summary>
        /// Create a vulkan instance
        /// </summary>
        void CreateGraphicsInstance();

        /// <summary>
        /// Determine if all requested layers are available.
        /// </summary>
        /// <returns>Returns true if layers are found, false otherwise</returns>
        bool CheckValidationLayerSupport();

        /// <summary>
        /// Determines what physical device to use for this Vulkan instance.
        /// </summary>
        void PickPhysicalDevice();

        /// <summary>
        /// Configure graphics API debug callbacks [Vulkan]
        /// </summary>
        void SetupDebugMessesages();

        std::vector<const char*> GetRequiredExtensions();

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
            VkDebugUtilsMessengerEXT* pDebugMessenger );

        void DestroyDebugUtilsMessengerEXT(
            VkInstance instance,
            VkDebugUtilsMessengerEXT debugMessenger,
            const VkAllocationCallbacks* pAllocator );

        /** The window that the game is being drawn to */
        GLFWwindow* m_Window = nullptr;

        /** The Vulkan instance */
        VkInstance m_Instance = VK_NULL_HANDLE;

        /** Physical device for Vulkan. Destroyed in cleanup. */
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;

        /** Debug message handler fo Vulkan */
        VkDebugUtilsMessengerEXT m_DebugMessenger;

        /** Width of the window that GLFW creates */
        int m_Width = 800;
        /** Height of the window that GLFW creates */
        int m_Height = 600;

#ifdef NDEBUG
        const bool m_EnableValidationLayers = false;
#else
        const bool m_EnableValidationLayers = true;
#endif

        const std::vector<const char*> m_ValidationLayers =
        {
            "VK_LAYER_KHRONOS_validation"
        };

    public:

        GLFWwindow* Window() const { return m_Window; }

    };

}	// namespace Fling