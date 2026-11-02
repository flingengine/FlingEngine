#include "FlingConfig.h"
#include "Instance.h"
#include "Logger.h"
#include "Utils/VkStringHelpers.h"
#include "Version.h"
#include "VulkanLoader.h"

#include <cstring>
#include <GLFW/glfw3.h>
#include <sstream>

namespace Fling
{
    Instance::Instance()
    {
        m_EnableValidationLayers = FlingConfig::GetBool("Vulkan", "EnableValidationLayers", false);
        F_LOG_TRACE("[Renderer] m_EnableValidationLayers is {}", (m_EnableValidationLayers ? "TRUE" : "FALSE"));

        CreateInstance();

#if FLING_DEBUG
        SetupDebugMessages();
#endif
    }

    void Instance::CreateInstance()
    {
        if (m_EnableValidationLayers && !CheckValidationLayerSupport())
        {
            F_LOG_WARN("Validation layers are requested, but not available!");
        }

        // Basic app data that we can modify
        VkApplicationInfo appInfo = {};
        // Most structs in Vulkan require you to specify the sType and
        // the pNext values
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Engine Bay 0.1";
        appInfo.applicationVersion =
            VK_MAKE_VERSION(Version::EngineVersion.Major, Version::EngineVersion.Minor, Version::EngineVersion.Patch);
        appInfo.pEngineName = "Fling Engine";
        appInfo.engineVersion =
            VK_MAKE_VERSION(Version::EngineVersion.Major, Version::EngineVersion.Minor, Version::EngineVersion.Patch);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // Instance creation info, similar to how DX11 worked
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // Because we are using GLFW as an extension interface for window creation
        // Vulkan needs to know how many extensions are available
        std::vector<const char*> extensions = GetRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Determine global validation layer count
        if (m_EnableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32>(m_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        // Create the vulkan instance! Throw exception if it fails
        if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to create the Vulkan instance!");
        }

        VulkanLoader::LoadInstance(m_Instance);

        // Find out what extensions are available
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> vk_extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, vk_extensions.data());

        F_LOG_TRACE("{} Extensions are supported!", extensionCount);
        for (const auto& extension : vk_extensions)
        {
            F_LOG_TRACE("\t{}", extension.extensionName);
        }
    }

    bool Instance::CheckValidationLayerSupport()
    {
        // Get the list of available validation layers
        // Validation layers are ways for us to choose what types of debugging
        // features we want from the Vulkan SDK
        uint32 layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : m_ValidationLayers)
        {
            bool layerFound = false;
            for (const VkLayerProperties& layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound)
            {
                return false;
            }
        }

        return true;
    }

    std::vector<const char*> Instance::GetRequiredExtensions()
    {
        uint32 glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (m_EnableValidationLayers)
        {
#if FLING_DEBUG
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
        }

        return extensions;
    }

#if FLING_DEBUG

    void Instance::SetupDebugMessages()
    {
        if (!m_EnableValidationLayers)
        {
            return;
        }

        F_LOG_TRACE("Setup VK Debug messages!");

        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugCallback;
        createInfo.pUserData = nullptr; // Optional

        if (CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to set up debug messenger!");
        }
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL Instance::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT t_messageSeverity,
                                                           VkDebugUtilsMessageTypeFlagsEXT t_messageType,
                                                           const VkDebugUtilsMessengerCallbackDataEXT* t_CallbackData,
                                                           void* t_UserData)
    {
        switch (t_messageSeverity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            F_LOG_WARN("Validation layer: {}", t_CallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
            F_LOG_ERROR("Validation layer error: {}", t_CallbackData->pMessage);
            FLING_BREAK();
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        default:
            F_LOG_TRACE("Validation layer: {}", t_CallbackData->pMessage);
            break;
        }

        return VK_FALSE;
    }

    VkResult Instance::CreateDebugUtilsMessengerEXT(VkInstance instance,
                                                    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                                    const VkAllocationCallbacks* pAllocator,
                                                    VkDebugUtilsMessengerEXT* pDebugMessenger)
    {
        PFN_vkCreateDebugUtilsMessengerEXT func =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        }
        else
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void Instance::DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                                 VkDebugUtilsMessengerEXT debugMessenger,
                                                 const VkAllocationCallbacks* pAllocator)
    {
        PFN_vkDestroyDebugUtilsMessengerEXT func =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            func(instance, m_DebugMessenger, pAllocator);
        }
    }

#endif // FLING_DEBUG

    Instance::~Instance()
    {
        // Debug messenger
        if (m_EnableValidationLayers)
        {
#if FLING_DEBUG
            DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
#endif
        }

        vkDestroyInstance(m_Instance, nullptr);
    }

    std::vector<VkPhysicalDevice> Instance::GetAllAvailableDevices() const
    {
        if (m_Instance == VK_NULL_HANDLE)
        {
            return {};
        }

        uint32 DeviceCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &DeviceCount, nullptr);

        if (DeviceCount == 0)
        {
            F_LOG_FATAL("Failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> Devices(DeviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &DeviceCount, Devices.data());
        return Devices;
    }

    VkPhysicalDevice Instance::GetBestPhysicalDevice() const
    {
        const std::vector<VkPhysicalDevice> AvailableDevices = GetAllAvailableDevices();
        return ChooseBestPhysicalDevice(AvailableDevices, GetEnabledExtensions());
    }

    VkPhysicalDevice Instance::ChooseBestPhysicalDevice(const std::vector<VkPhysicalDevice>& t_AvailableDevices,
                                                        const std::vector<const char*>& t_EnabledExtensions)
    {
        // Lambda for scoring a physical device
        auto ScoreDeviceLambda = [&t_EnabledExtensions](VkPhysicalDevice t_Device) -> int32
        {
            int32 Score = 0;

            // Get the extension support of this device
            uint32_t ExtensionPropertyCount;
            vkEnumerateDeviceExtensionProperties(t_Device, nullptr, &ExtensionPropertyCount, nullptr);
            std::vector<VkExtensionProperties> ExtensionProperties(ExtensionPropertyCount);
            vkEnumerateDeviceExtensionProperties(
                t_Device, nullptr, &ExtensionPropertyCount, ExtensionProperties.data());

            // Check to make sure that this device supports all needed extensions
            for (const char* CurrentExtension : t_EnabledExtensions)
            {
                bool ExtensionFound = false;

                for (const VkExtensionProperties& Extension : ExtensionProperties)
                {
                    if (strcmp(CurrentExtension, Extension.extensionName) == 0)
                    {
                        ExtensionFound = true;
                        break;
                    }
                }

                // If not, then just return 0 because we won't want to use this device
                if (!ExtensionFound)
                {
                    return 0;
                }
            }

            // Obtain the device properties of the current device being rated
            VkPhysicalDeviceProperties physicalDeviceProperties;
            vkGetPhysicalDeviceProperties(t_Device, &physicalDeviceProperties);

            // Prefer real GPUs over software rasterizers (e.g. llvmpipe), and discrete over integrated.
            switch (physicalDeviceProperties.deviceType)
            {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                Score += 100000;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                Score += 50000;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                Score += 25000;
                break;
            default:
                break;
            }

            // Gives a higher score to devices with a higher maximum texture size.
            Score += static_cast<int32>(physicalDeviceProperties.limits.maxImageDimension2D);

            return Score;
        };

        VkPhysicalDevice BestDevice = VK_NULL_HANDLE;
        int32 BestScore = 0;

        std::stringstream AvailableDevicesStr;
        AvailableDevicesStr << "Available physical VK devices: \n";

        for (const VkPhysicalDevice& Device : t_AvailableDevices)
        {
            const int32 DeviceScore = ScoreDeviceLambda(Device);
            if (DeviceScore > BestScore)
            {
                BestScore = DeviceScore;
                BestDevice = Device;
            }

            AvailableDevicesStr << Utils::DeviceToString(Device);
            AvailableDevicesStr << "\n\tscore: " << DeviceScore << "\n";
        }

        F_LOG_TRACE("{}", AvailableDevicesStr.str());

        return BestDevice;
    }
} // namespace Fling
