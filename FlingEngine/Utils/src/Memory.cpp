#include "pch.h"
#include "Memory.h"
#include "FlingExports.h"

namespace Fling
{
    void* AlignedAlloc(size_t t_Size, size_t t_Alignment)
    {
        void* data = nullptr;
#if FLING_WINDOWS
	data = _aligned_malloc(t_Size, t_Alignment);
#else
	int res = posix_memalign(&data, t_Alignment, t_Size);
	if (res != 0)
    {
		data = nullptr;
    }
#endif
        return data;
    }

    void AlignedFree(void* t_Data)
    {
#if FLING_WINDOWS
	_aligned_free(t_Data);
#else
    free(t_Data);
#endif
    }


}