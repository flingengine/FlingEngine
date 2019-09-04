#include "pch.h"
#include "Image.h"

namespace Fling
{
    Image::Image(Guid t_ID, UINT32 t_Width, UINT32 t_Height)
        : Resource(t_ID)
        , m_Width(t_Width)
        , m_Height(t_Height)
    {
    }
} // namespace Fling