#pragma once

#include "FlingVulkan.h"

namespace Fling
{

    struct FrameBufferAttachment
    {
        VkImage Image;
        VkDeviceMemory Mem;
        VkImageView View;
        VkFormat Format;

        void Release()
        {
            vkDestroyImage(device, image, nullptr);
			vkDestroyImageView(device, view, nullptr);
			vkFreeMemory(device, mem, nullptr);  
        }
    };

    class FrameBuffer
    {
    public: 
        FrameBuffer(INT32 t_Width, INT32 t_Height);
        ~FrameBuffer();

        void SizeSize(INT32 w, INT32 h)
		{
			this->width = w;
			this->height = h;
		}

		void Release(VkDevice device)
		{
			vkDestroyFramebuffer(device, frameBuffer, nullptr);
			vkDestroyRenderPass(device, renderPass, nullptr);
		}

        void AddAttachment();

    private:
        INT32 width = 0;
        INT32 height = 0;
		VkFramebuffer frameBuffer;

		std::vector<FrameBufferAttachment> m_Attachments;
		
        VkRenderPass renderPass;
	
    };
}   // namespace Fling