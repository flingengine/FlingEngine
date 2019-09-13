#include "pch.h"
#include "PhyscialDevice.h"
#include "Instance.h"

namespace Fling
{
    PhysicalDevice::PhysicalDevice(Instance* t_Instance, VkSurfaceKHR t_Surface)
        : m_Instance(t_Instance)
        , m_Surface(t_Surface)
    {
        assert(m_Instance);
        PickPhysicalDevice();
    }

    void PhysicalDevice::PickPhysicalDevice()
    {
        // Enumerate available devices
		UINT32 DeviceCount = 0;
		vkEnumeratePhysicalDevices( m_Instance->GetRawVkInstance(), &DeviceCount, nullptr );

		if( DeviceCount == 0 )
		{
            F_LOG_FATAL( "Failed to find GPU's with Vulkan support!" );
		}

        std::vector<VkPhysicalDevice> Devices( DeviceCount );
        vkEnumeratePhysicalDevices( m_Instance->GetRawVkInstance(), &DeviceCount, Devices.data() );
        
        // Find the best device for this application
        PhysicalDeviceRating MaxRating = {};
        for( const VkPhysicalDevice& Device : Devices )
        {
            PhysicalDeviceRating Rating = GetDeviceRating( Device );
            if( Rating.Score > 0 && Rating.Score > MaxRating.Score )
            {
                MaxRating.Score = Rating.Score;
                MaxRating.DeviceFeatures = Rating.DeviceFeatures;
                MaxRating.DeviceProperties = Rating.DeviceProperties;
                m_PhysicalDevice = Device;
            }
        }

        if( m_PhysicalDevice == VK_NULL_HANDLE )
        {
            F_LOG_FATAL( "Failed to find a suitable GPU!" );
        }
    }

    QueueFamilyIndices PhysicalDevice::FindQueueFamilies(VkPhysicalDevice t_PhysDevice, VkSurfaceKHR t_Surface)
    {
        QueueFamilyIndices Indecies = {};

        UINT32 QueueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties( t_PhysDevice, &QueueFamilyCount, nullptr );

        std::vector<VkQueueFamilyProperties> QueueFamilies( QueueFamilyCount );
        vkGetPhysicalDeviceQueueFamilyProperties( t_PhysDevice, &QueueFamilyCount, QueueFamilies.data() );
        // Set the family flags we are interested in
        int i = 0;
        for( const VkQueueFamilyProperties& Family : QueueFamilies )
        {
            if (Family.queueCount > 0)
            {
                Indecies.GraphicsFamily = ((Family.queueFlags & VK_QUEUE_GRAPHICS_BIT) ? i : 0);
                
                VkBool32 PresentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(t_PhysDevice, i, t_Surface, &PresentSupport);

                Indecies.PresentFamily = PresentSupport ? i : 0;
            }
                    
            if( Indecies.IsComplete() )
            {
                break;
            }
            i++;
        }

        return Indecies;
    }
    
    PhysicalDeviceRating PhysicalDevice::GetDeviceRating( VkPhysicalDevice const t_Device )
    {
        PhysicalDeviceRating Rating = {};

        vkGetPhysicalDeviceProperties( t_Device, &Rating.DeviceProperties );
        vkGetPhysicalDeviceFeatures( t_Device, &Rating.DeviceFeatures );

        // Necessary application features, if the device doesn't have one then return 0
        if( !Rating.DeviceFeatures.geometryShader )
        {
            return Rating;
        }

        // Ensure that this devices supports all necessary extensions
        //if (!CheckDeviceExtensionSupport(t_Device))
        //{
        //    return 0;
        //}

        // This device must have swap chain support
        //SwapChainSupportDetails SwapChainSupport = QuerySwapChainSupport(t_Device);
        //if (SwapChainSupport.Formats.empty() || SwapChainSupport.PresentModes.empty())
        //{
        //  return 0;
        //}

        // Favor discrete GPU's
        if( Rating.DeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU )
        {
            Rating.Score += 500;
        }

        // Favor complete queue sets
        QueueFamilyIndices QueueFamily = FindQueueFamilies();
        if( QueueFamily.IsComplete() )
        {
            Rating.Score += 500;
        }

        return Rating;
    }

    PhysicalDevice::~PhysicalDevice()
    {
        // Will get cleaned up when the instance is destroyed
    }

}   // namespace Fling