#pragma once

#include <stdlib.h>

namespace Fling
{
    void* AlignedAlloc(size_t t_Size, size_t t_Alignment);

    void AlignedFree(void* t_Data);

}   // namespace Fling