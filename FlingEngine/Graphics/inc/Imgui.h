#pragma once 
#include "FlingVulkan.h"
#include "Buffer.h"

namespace Fling 
{
    class ImGUI 
    {
        public:
            
        private:
            VkSampler m_sampler;
            Buffer m_vertexBuffer;
            Buffer m_indexBuffer;            
            INT32 vertexCount;
            INT32 indexCount;
            VkDeviceMemory fontMemory = VK_NULL_HANDLE;
            VkImage fontImage = VK_NULL_HANDLE;
    }
}