#include "pch.h"
#include "LogicalDevice.h"
#include "Instance.h"
#include "PhyscialDevice.h"

namespace Fling
{
    LogicalDevice::LogicalDevice(Instance* t_Instance, PhysicalDevice* t_PhysDevice, const VkSurfaceKHR t_Surface)
        : m_Instance(t_Instance)
		, m_PhysicalDevice(t_PhysDevice)
        , m_Surface(t_Surface)
    {
		CreateQueueIndecies();

        CreateDevice();
    }

	void LogicalDevice::CreateQueueIndecies()
	{
		UINT32 QueueFamilyCount = 0;
		VkPhysicalDevice PhysDevice = m_PhysicalDevice->GetVkPhysicalDevice();
		vkGetPhysicalDeviceQueueFamilyProperties(PhysDevice, &QueueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(PhysDevice, &QueueFamilyCount, QueueFamilies.data());

		std::optional<UINT32> graphicsFamily;
		std::optional<UINT32> presentFamily;
		std::optional<UINT32> computeFamily;
		std::optional<UINT32> transferFamily;

		for (uint32_t i = 0; i < QueueFamilyCount; ++i)
		{
			// Check for graphics support.
			if (QueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				graphicsFamily = i;
				m_GraphicsFamily = i;
				m_SupportedQueues |= VK_QUEUE_GRAPHICS_BIT;
			}

			// Check for presentation support.
			VkBool32 presentSupport;
			vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice->GetVkPhysicalDevice(), i, m_Surface, &presentSupport);

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
        std::set<UINT32> UniqueQueueFamilies = { m_GraphicsFamily, m_PresentFamily };

        // Generate the CreatinInfo for each queue family 
		std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;

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
		DevicesFeatures.samplerAnisotropy = VK_TRUE;
		DevicesFeatures.sampleRateShading = VK_TRUE;


        // Device creation 
        VkDeviceCreateInfo CreateInfo = {};
        CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        CreateInfo.queueCreateInfoCount = static_cast<UINT32>(QueueCreateInfos.size());
        CreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
        CreateInfo.pEnabledFeatures = &DevicesFeatures;

        // Set the enabled extensions
        CreateInfo.enabledExtensionCount = static_cast<UINT32>(m_Instance->GetEnabledExtensions().size());
        CreateInfo.ppEnabledExtensionNames = m_Instance->GetEnabledExtensions().data();

        if( m_Instance->IsValidationEnabled() ) 
        {
            CreateInfo.enabledLayerCount = m_Instance->EnabledValidationLayerCount();
            CreateInfo.ppEnabledLayerNames = m_Instance->GetEnabledValidationLayers().data();
        }
        else 
        {
            CreateInfo.enabledLayerCount = 0;
        }

        if(vkCreateDevice(m_PhysicalDevice->GetVkPhysicalDevice(), &CreateInfo, nullptr, &m_Device) != VK_SUCCESS)
        {
            F_LOG_FATAL( "Failed to create logical Device!" );
        }

        vkGetDeviceQueue(m_Device, m_GraphicsFamily, 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, m_PresentFamily, 0, &m_PresentQueue);
    }

	void LogicalDevice::WaitForIdle()
	{
		vkDeviceWaitIdle(m_Device);
	}

    LogicalDevice::~LogicalDevice()
    {
		WaitForIdle();

        vkDestroyDevice(m_Device, nullptr);
    }
}   // namespace Fling