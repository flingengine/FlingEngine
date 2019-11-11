#include "FreeList.h"

namespace Fling
{
    FreeList::FreeList(void* t_Start, void* t_End, size_t elmSize, size_t t_NumElms, size_t alignment, size_t offset)
    {
        union
        {
            void* as_void;
            char* as_char;
            FreeList* as_self;
        };

        as_void = t_Start;

        // assume as_self points to the first entry in the free list
        m_Next = as_self;
        as_char += elmSize;

        // initialize the free list - make every m_next of each element point to the next element in the list
        FreeList* runner = m_Next;
        for (size_t i = 1; i < t_NumElms; ++i)
        {
            runner->m_Next = as_self;
            runner = as_self;
            as_char += elmSize;
        }
        
        runner->m_Next = nullptr;
    }

    void* FreeList::Obtain()
    {
        if(m_Next == nullptr)
        {
            return nullptr;
        }

        FreeList* head = m_Next;
        m_Next = head->m_Next;
        return head;
    }

    void FreeList::Return(void* t_Ptr)
    {
        FreeList* head = static_cast<FreeList*>(t_Ptr);
        head->m_Next = m_Next;
        m_Next = head;
    }
}