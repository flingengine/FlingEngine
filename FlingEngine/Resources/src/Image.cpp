#include "pch.h"

// Define the STB library image implementation here. Make sure 
// this is ONLY in this file 
#define STB_IMAGE_IMPLEMENTATION
#include "Image.h"

namespace Fling
{
    Image::Image(Guid t_ID)
        : Resource(t_ID)
    {
        const std::string Filepath = GetFilepathReleativeToAssets();
        // Load the image from STB
        m_PixelData = stbi_load(
            Filepath.c_str(), 
            &m_Width,
            &m_Height,
            &m_Channels, 
            STBI_rgb_alpha
        );
        
        if(!m_PixelData)
        {
            F_LOG_ERROR("Failed to load image file: {}", Filepath);
        }
        else
        {
            F_LOG_TRACE("Loaded image file: {}", Filepath);
        }
    }

    Image::~Image()
    {
        // Cleanup pixel data if we have to
        stbi_image_free(m_PixelData);
    }
} // namespace Fling