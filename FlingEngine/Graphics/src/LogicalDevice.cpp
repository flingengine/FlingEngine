#include "pch.h"
#include "LogicalDevice.h"
#include "Instance.h"
#include "PhyscialDevice.h"

namespace Fling
{
    LogicalDevice::LogicalDevice(Instance* t_Instance, PhysicalDevice* t_PhysicalDevice)
        : m_Instance(t_Instance)
        , m_PhysDevice(t_PhysicalDevice)
    {
        CreateDevice();
    }

    void LogicalDevice::CreateDevice()
    {
        // Queue creation
        QueueFamilyIndices Indecies = m_PhysDevice->FindQueueFamilies();
  
        std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
        std::set<UINT32> UniqueQueueFamilies = { Indecies.GraphicsFamily, Indecies.PresentFamily };

        // Generate the CreatinInfo for each queue family 
        float Priority = 1.0f;
        for (const UINT32 fam : UniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo QueueCreateInfo = {};
            QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QueueCreateInfo.queueFamilyIndex = fam;
            QueueCreateInfo.queueCount = 1;
            QueueCreateInfo.pQueuePriorities = &Priority;
            QueueCreateInfos.push_back(QueueCreateInfo);
        }

        VkPhysicalDeviceFeatures DevicesFeatures = {};

        // Device creation 
        VkDeviceCreateInfo CreateInfo = {};
        CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        CreateInfo.queueCreateInfoCount = static_cast<UINT32>(QueueCreateInfos.size());
        CreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
        CreateInfo.pEnabledFeatures = &DevicesFeatures;

        // Set the enabled extensions
        CreateInfo.enabledExtensionCount = static_cast<UINT32>(m_DeviceExtensions.size());
        CreateInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

        if( m_Instance->IsValidationEnabled() ) 
        {
            CreateInfo.enabledLayerCount = m_Instance->EnabledValidationLayerCount();
            CreateInfo.ppEnabledLayerNames = m_Instance->GetEnabledValidationLayers().data();
        }
        else 
        {
            CreateInfo.enabledLayerCount = 0;
        }

        if( vkCreateDevice( m_PhysDevice->GetVkPhysicalDevice(), &CreateInfo, nullptr, &m_Device ) != VK_SUCCESS ) 
        {
            F_LOG_FATAL( "failed to create logical Device!" );
        }

        vkGetDeviceQueue( m_Device, Indecies.GraphicsFamily, 0, &m_GraphicsQueue );
        vkGetDeviceQueue(m_Device, Indecies.PresentFamily, 0, &m_PresentQueue);
    }

    void LogicalDevice::PrepShutdown()
    {
        vkDeviceWaitIdle(m_Device);
    }

    LogicalDevice::~LogicalDevice()
    {
        vkDestroyDevice(m_Device, nullptr);
    }
}   // namespace Fling