#pragma once

namespace Fling
{
    /**
     * @brief   Helpful for allocating/freeing objects of a certain
     *          size which have to be created/destroeyed dynamically
     * 
     * @see     https://blog.molecular-matters.com/2012/09/17/memory-allocation-strategies-a-pool-allocator/
     */
    class FreeList
    {
    public:
        FreeList(void* t_Start, void* t_End, size_t t_ElmSize, size_t t_NumElms, size_t t_Alignment = 8, size_t t_Offset = 0);

        inline void* Obtain();

        inline void Return(void* t_Ptr);

    private:
        /** Alias in memory to the next available block. Nullptr if none are available */
        FreeList* m_Next = nullptr;
    };
}   // namespace Fling