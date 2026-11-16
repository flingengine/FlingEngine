#pragma once

#include "FlingTypes.h"
#include "FlingVulkan.h"
#include "Strings/String.h"

#include <vector>

namespace Fling
{
    class LogicalDevice;

    namespace FrameCapture
    {
        /**
         * Copy a color image that the GPU has finished drawing to host memory as RGBA8 (4
         * bytes a pixel, rows top to bottom; Screenshot requests). The image must have been
         * made with VK_IMAGE_USAGE_TRANSFER_SRC_BIT. It is left in t_Layout. Alpha is written
         * as opaque: the frame's clear alpha is not meant to be seen.
         *
         * Waits for the graphics queue to be idle, so call it only for a capture.
         *
         * @param t_Image   The image to copy
         * @param t_Format  VK_FORMAT_B8G8R8A8_* or VK_FORMAT_R8G8B8A8_* (other formats are refused)
         * @param t_Extent  The size of the image
         * @param t_Layout  The layout the image is in now (PRESENT_SRC or TRANSFER_SRC)
         * @param t_OutRgba Receives the pixels
         * @param t_OutError Receives why it failed
         * @return False if the format is not supported
         */
        bool ReadImageRgba(LogicalDevice& t_Device,
                           VkImage t_Image,
                           VkFormat t_Format,
                           VkExtent2D t_Extent,
                           VkImageLayout t_Layout,
                           std::vector<uint8>& t_OutRgba,
                           String& t_OutError);

        /**
         * Encode RGBA8 pixels (4 bytes a pixel, rows top to bottom) as a PNG file. Missing
         * folders are made. Pure CPU work: the tests call it with no GPU.
         */
        bool WritePng(StringView t_Path, uint32 t_Width, uint32 t_Height, const uint8* t_Rgba, String& t_OutError);
    } // namespace FrameCapture
} // namespace Fling
