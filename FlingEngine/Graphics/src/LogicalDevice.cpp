#include "FlingIntegers.h"
#include "FrameSync.h"
#include "Instance.h"
#include "Logger.h"
#include "LogicalDevice.h"
#include "PhyscialDevice.h"
#include "VulkanLoader.h"

#include <optional>
#include <set>

namespace Fling
{
    LogicalDevice::LogicalDevice(Instance* t_Instance, PhysicalDevice* t_PhysDevice, const VkSurfaceKHR t_Surface)
        : m_Instance(t_Instance), m_PhysicalDevice(t_PhysDevice), m_Surface(t_Surface)
    {
        CreateQueueIndecies();

        CreateDevice();
    }

    void LogicalDevice::CreateQueueIndecies()
    {
        uint32 QueueFamilyCount = 0;
        VkPhysicalDevice PhysDevice = m_PhysicalDevice->GetVkPhysicalDevice();
        vkGetPhysicalDeviceQueueFamilyProperties(PhysDevice, &QueueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(PhysDevice, &QueueFamilyCount, QueueFamilies.data());
        m_FamilyQueueCounts.resize(QueueFamilyCount);
        for (uint32 i = 0; i < QueueFamilyCount; ++i)
        {
            m_FamilyQueueCounts[i] = QueueFamilies[i].queueCount;
        }

        std::optional<uint32> graphicsFamily;
        std::optional<uint32> presentFamily;
        std::optional<uint32> computeFamily;
        std::optional<uint32> transferFamily;

        for (uint32 i = 0; i < QueueFamilyCount; ++i)
        {
            // Check for graphics support.
            if (QueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                graphicsFamily = i;
                m_GraphicsFamily = i;
                m_SupportedQueues |= VK_QUEUE_GRAPHICS_BIT;
            }

            // Check for presentation support. With no surface (offscreen) nothing is presented,
            // and the "present" queue is the graphics queue
            VkBool32 presentSupport = VK_FALSE;
            if (m_Surface != VK_NULL_HANDLE)
            {
                vkGetPhysicalDeviceSurfaceSupportKHR(
                    m_PhysicalDevice->GetVkPhysicalDevice(), i, m_Surface, &presentSupport);
            }
            else
            {
                presentSupport = (QueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ? VK_TRUE : VK_FALSE;
            }

            if (QueueFamilies[i].queueCount > 0 && presentSupport)
            {
                presentFamily = i;
                m_PresentFamily = i;
            }

            // Check for compute support.
            if (QueueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                computeFamily = i;
                m_ComputeFamily = i;
                m_SupportedQueues |= VK_QUEUE_COMPUTE_BIT;
            }

            // Check for transfer support.
            if (QueueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                transferFamily = i;
                m_TransferFamily = i;
                m_SupportedQueues |= VK_QUEUE_TRANSFER_BIT;
            }

            if (graphicsFamily && presentFamily && computeFamily && transferFamily)
            {
                break;
            }
        }

        if (!graphicsFamily)
        {
            F_LOG_FATAL("Failed to find queue family supporting VK_QUEUE_GRAPHICS_BIT");
        }
    }

    void LogicalDevice::CreateDevice()
    {
        std::set<uint32> UniqueQueueFamilies = {m_GraphicsFamily, m_PresentFamily};

        // Generate the CreatinInfo for each queue family
        std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;

        float Priority[2] = {1.0f, 1.0f};
        // A second queue of the graphics family, for presenting from its own thread (a queue is externally
        // synchronized, and vkQueuePresentKHR can block until the GPU has finished the frame)
        m_SeparatePresentQueue = m_GraphicsFamily == m_PresentFamily && m_Surface != VK_NULL_HANDLE &&
                                 m_FamilyQueueCounts[m_GraphicsFamily] >= 2;
        for (const uint32 fam : UniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo QueueCreateInfo = {};
            QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QueueCreateInfo.queueFamilyIndex = fam;
            QueueCreateInfo.queueCount = (fam == m_GraphicsFamily && m_SeparatePresentQueue) ? 2 : 1;
            QueueCreateInfo.pQueuePriorities = Priority;
            QueueCreateInfos.push_back(QueueCreateInfo);
        }

        VkPhysicalDeviceFeatures DevicesFeatures = {};
        DevicesFeatures.samplerAnisotropy = VK_TRUE;
        DevicesFeatures.sampleRateShading = VK_TRUE;

        // Device creation
        VkDeviceCreateInfo CreateInfo = {};
        CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        CreateInfo.queueCreateInfoCount = static_cast<uint32>(QueueCreateInfos.size());
        CreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
        CreateInfo.pEnabledFeatures = &DevicesFeatures;

        // Set the enabled extensions
        CreateInfo.enabledExtensionCount = static_cast<uint32>(m_Instance->GetEnabledExtensions().size());
        CreateInfo.ppEnabledExtensionNames = m_Instance->GetEnabledExtensions().data();

        if (m_Instance->IsValidationEnabled())
        {
            CreateInfo.enabledLayerCount = m_Instance->EnabledValidationLayerCount();
            CreateInfo.ppEnabledLayerNames = m_Instance->GetEnabledValidationLayers().data();
        }
        else
        {
            CreateInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(m_PhysicalDevice->GetVkPhysicalDevice(), &CreateInfo, nullptr, &m_Device) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to create logical Device!");
        }

        VulkanLoader::LoadDevice(m_Device);

        vkGetDeviceQueue(m_Device, m_GraphicsFamily, 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, m_PresentFamily, m_SeparatePresentQueue ? 1 : 0, &m_PresentQueue);
    }

    void LogicalDevice::WaitForIdle() const
    {
        // A thread that presents holds its queue, which the wait needs to be free
        if (m_BeforeIdle)
        {
            m_BeforeIdle();
        }
        vkDeviceWaitIdle(m_Device);
        FrameSync::MarkIdle();
    }

    LogicalDevice::~LogicalDevice()
    {
        WaitForIdle();

        vkDestroyDevice(m_Device, nullptr);
    }
} // namespace Fling