#pragma once

#include "Resource.h"

namespace Fling
{
    /**
     * @brief   An image represents a 2D file that has data about each pixel in the image
     */
    class Image : public Resource
    {
    public:

        explicit Image(Guid t_ID, UINT32 t_Width, UINT32 t_Height);

        UINT32 GetWidth() const { return m_Width; }
        UINT32 GetHeight() const { return m_Height; }

    private:

        // #TODO: Use the stb image loading lib and wrap the data that I will need from that here

        /** Width of this image */
        UINT32 m_Width = 0;

        /** Height of this image */
        UINT32 m_Height = 0;
    };
}   // namespace Fling