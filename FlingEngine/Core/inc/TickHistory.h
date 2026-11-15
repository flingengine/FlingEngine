#pragma once

#include "FlingTypes.h"

#include <array>
#include <cassert>
#include <cstddef>

namespace Fling
{
    /**
     * The values of the last N ticks, keyed by tick number: a ring with one slot per tick
     * (slot = tick % N). The ledger keeps its tick records in one; a per-tick state
     * history uses the same type.
     *
     * Push the ticks in order, one after the other (tick numbers never skip). Find is
     * O(1). A tick older than the last N is gone.
     */
    template <typename T, size_t N>
    class TickHistory
    {
    public:
        static_assert(N > 0, "A TickHistory keeps at least one tick");

        static constexpr size_t Capacity = N;

        /** Store the value of t_Tick. t_Tick must be one more than the newest tick (any tick if empty). */
        void Push(const uint64 t_Tick, const T& t_Value)
        {
            assert((m_Count == 0 || t_Tick == m_Newest + 1) && "Ticks go into a TickHistory in order");
            Entry& Slot = m_Entries[t_Tick % N];
            Slot.Tick = t_Tick;
            Slot.Value = t_Value;
            m_Newest = t_Tick;
            ++m_Count;
        }

        size_t Size() const { return m_Count < N ? static_cast<size_t>(m_Count) : N; }

        bool Empty() const { return m_Count == 0; }

        /** The value t_Index after the oldest one that is kept (0 is the oldest). */
        const T& At(const size_t t_Index) const
        {
            assert(t_Index < Size());
            return m_Entries[(OldestTick() + t_Index) % N].Value;
        }

        /** The newest value, or null when it is empty. */
        const T* Latest() const { return Empty() ? nullptr : &m_Entries[m_Newest % N].Value; }

        /** The value of a tick, or null if it is not kept (too old, not pushed yet, or 0: no tick). */
        const T* Find(const uint64 t_Tick) const
        {
            const Entry& Slot = m_Entries[t_Tick % N];
            return t_Tick > 0 && !Empty() && t_Tick <= m_Newest && Slot.Tick == t_Tick ? &Slot.Value : nullptr;
        }

        /** The number of values pushed since the start (also those that are no longer kept). */
        uint64 TotalWritten() const { return m_Count; }

    private:
        struct Entry
        {
            uint64 Tick = 0;
            T Value{};
        };

        uint64 OldestTick() const { return m_Newest + 1 - Size(); }

        std::array<Entry, N> m_Entries{};
        uint64 m_Newest = 0;
        uint64 m_Count = 0;
    };
} // namespace Fling
