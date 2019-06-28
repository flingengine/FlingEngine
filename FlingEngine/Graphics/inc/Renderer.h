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

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR Capabilities;
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR> PresentModes;
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
        void CreateGameWindow( const UINT32 t_width, const UINT32 t_height );

        /// <summary>
        /// Get a rating of how good this device is for this application.
        /// Scores range from 0 - 1000
        /// </summary>
        /// <param name="t_Device">Device to consider</param>
        /// <returns>Score on a scale of 0 to 1000</returns>
        UINT16 GetDeviceRating( VkPhysicalDevice const t_Device );

        /// <summary>
        /// Find what queue families are supported by a given 
        /// </summary>
        /// <param name="t_Device">Device to check</param>
        /// <returns>Queue family flags</returns>
        QueueFamilyIndices FindQueueFamilies( VkPhysicalDevice const t_Device );

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
        /// Create the logical vulkan device
        /// </summary>
        void CreateLogicalDevice();

        /// <summary>
        /// Configure graphics API debug callbacks [Vulkan]
        /// </summary>
        void SetupDebugMessesages();

        /**
        * Create the surface for Vulkan to use for integration with the window system
        * this surface can have an effect on the selection of physical device
        */
        void CreateSurface();

        /**
        * Create the swap chain and select the format, present mode, and extents
        */
        void CreateSwapChain();

        /**
        * Check the swap chain support of a given device
        * @param 	The device to check support on
        *
        * @return   Details of the the swap chain support on this device
        */
        SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice t_Device);

        /**
        * Choose a swap chain format based on the available formats. Prefer to format
        * that has VK_FORMAT_B8G8R8A8_UNORM and VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, otherwise
        * get the first available. 
        * 
        * @param    Available swap chain formats
        *
        * @return   Best swap chain surface formate based on the available ones
        */
        VkSurfaceFormatKHR ChooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& t_AvailableFormats);

        /**
        * Choose a present mode for the swap chain based on the given formats. Prefer VK_PRESENT_MODE_MAILBOX_KHR
        * If none are available, than return VK_PRESENT_MODE_FIFO_KHR or VK_PRESENT_MODE_IMMEDIATE_KHR based on support
        * 
        * @param    Vector of available formats
        *
        * @return   Preferred present mode from the available formats   	
        */
        VkPresentModeKHR ChooseSwapChainPresentMode(const std::vector< VkPresentModeKHR>& t_AvialableFormats);

        /**
        * Determine the best match extents based on our window width and height
        * 
        * @param 	t_Capabilies    The available capabilities of the swap chain on this device
        *
        * @return   Extents with the best matching resolution
        */
        VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& t_Capabilies);

        std::vector<const char*> GetRequiredExtensions();

        /**
        * Check if the given device supports the extensinos that this application requires
        * 
        * @param t_Device       The device to check  		
        *
        * @return True if device supports our listed extensions
        * @see Renderer::m_DeviceExtensions
        */
        bool CheckDeviceExtensionSupport(VkPhysicalDevice t_Device);

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

        /** Logical Vulkan device */
        VkDevice m_Device = VK_NULL_HANDLE;

        /** Handle for the graphics queue */
        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;

        /** Handle to the presentation queue */
        VkQueue m_PresentQueue = VK_NULL_HANDLE;

        /** Debug message handler for Vulkan */
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;

        /** Handle to the surface extension used to interact with the windows system */
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

        /** The swap chain of this renderer */
        VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;

        VkExtent2D m_SwapChainExtents;

        VkSurfaceFormatKHR m_SwapChainFormat;

        /** Width of the window that GLFW creates.  @see Renderer::CreateGameWindow */
        UINT32 m_WindowWidth = 800;
        /** Height of the window that GLFW creates  @see Renderer::CreateGameWindow */
        UINT32 m_WindowHeight = 600;

#ifdef NDEBUG
        const bool m_EnableValidationLayers = false;
#else
        const bool m_EnableValidationLayers = true;
#endif

        const std::vector<const char*> m_ValidationLayers =
        {
            "VK_LAYER_KHRONOS_validation"
        };

        /** Device extension support for the swap chain */
        const std::vector<const char*> m_DeviceExtensions = 
        {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        /** The images inside of the swap chain */
        std::vector<VkImage> m_SwapChainImages;

    public:

        GLFWwindow* Window() const { return m_Window; }

    };

}	// namespace Fling