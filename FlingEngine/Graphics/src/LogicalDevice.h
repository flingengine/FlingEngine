#pragma once

#include "FlingTypes.h"
#include "FlingVulkan.h"

#include <functional>
#include <vector>

namespace Fling
{
    class PhysicalDevice;
    class Instance;

    /**
     * A logical device represents the application view of the device
     */
    class LogicalDevice
    {
    public:
        explicit LogicalDevice(class Instance* t_Instance,
                               class PhysicalDevice* t_PhysDevice,
                               const VkSurfaceKHR t_Surface);

        ~LogicalDevice();

        const VkDevice& GetVkDevice() const { return m_Device; }

        const VkQueue& GetGraphicsQueue() const { return m_GraphicsQueue; }
        const VkQueue& GetPresentQueue() const { return m_PresentQueue; }

        const VkQueueFlags& GetSupportedQueues() const { return m_SupportedQueues; }

        const PhysicalDevice* GetPhysicalDevice() const { return m_PhysicalDevice; }
        const Instance* GetInstance() const { return m_Instance; }

        uint32 GetGraphicsFamily() const { return m_GraphicsFamily; }
        uint32 GetPresentFamily() const { return m_PresentFamily; }

        /**
         * True if the present queue is a queue of its own (the graphics family has two), so another thread can
         * present while this one submits. vkQueuePresentKHR can block until the GPU has finished the frame.
         */
        bool HasSeparatePresentQueue() const { return m_SeparatePresentQueue; }

        /**
         * A function that WaitForIdle calls before it waits: the thread that presents makes its queue free there.
         * Main (render) thread only.
         */
        void SetBeforeIdle(std::function<void()> t_Function) { m_BeforeIdle = std::move(t_Function); }

        /** Wait until the GPU is idle. Every frame before the current one is finished then (FrameSync). */
        void WaitForIdle() const;

    private:
        /** The vulkan logical device */
        VkDevice m_Device = VK_NULL_HANDLE;

        const Instance* m_Instance;
        const PhysicalDevice* m_PhysicalDevice;
        const VkSurfaceKHR m_Surface;

        /** Handle for the graphics queue */
        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;

        /** Handle to the presentation queue */
        VkQueue m_PresentQueue = VK_NULL_HANDLE;

        /** Queue families */
        VkQueueFlags m_SupportedQueues{};
        bool m_SeparatePresentQueue = false;
        std::vector<uint32> m_FamilyQueueCounts;
        std::function<void()> m_BeforeIdle;
        uint32 m_GraphicsFamily = 0;
        uint32 m_PresentFamily = 0;
        uint32 m_ComputeFamily = 0;
        uint32 m_TransferFamily = 0;

        /**
         * Get what queue Indecies/families this device should use
         */
        void CreateQueueIndecies();

        /**
         * Create the Vk resoruces for this logical device
         */
        void CreateDevice();
    };
} // namespace Fling