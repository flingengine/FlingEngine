#pragma once

#include "FlingTypes.h"

#include <array>
#include <atomic>
#include <utility>

namespace Fling
{
    /**
     * Three values, for one writer thread and one reader thread: the newest value
     * wins. The writer fills WriteSlot and calls Publish. The reader calls
     * AcquireLatest, then reads ReadSlot until its next AcquireLatest.
     *
     * Publish and AcquireLatest are one atomic exchange each, so neither thread
     * ever waits for the other: no lock, no spin. A value the reader did not
     * acquire before the next Publish is dropped, so each value must be complete
     * on its own (full state, not a delta). The slots are reused forever, so a
     * value that keeps its capacity (a vector) does not allocate after the first
     * few rounds. A slot that the writer gets back holds an older value: the
     * writer overwrites all of it.
     *
     * Only the writer thread may call WriteSlot and Publish. Only the reader thread
     * may call AcquireLatest and ReadSlot.
     */
    template <typename T>
    class TripleBuffer
    {
    public:
        TripleBuffer() = default;

        /** Each slot starts as a copy of initial. */
        explicit TripleBuffer(const T& initial) : m_Slots{initial, initial, initial} {}

        TripleBuffer(const TripleBuffer&) = delete;
        TripleBuffer& operator=(const TripleBuffer&) = delete;

        /** Writer: the slot to fill. It is not visible to the reader until Publish. */
        T& WriteSlot() { return m_Slots[m_WriteIndex]; }

        /** Writer: make the write slot the newest value, and take the older ready slot as the next write slot. */
        void Publish()
        {
            const uint8 previous =
                m_Ready.exchange(static_cast<uint8>(m_WriteIndex | NewBit), std::memory_order_acq_rel);
            m_WriteIndex = static_cast<uint8>(previous & IndexMask);
            m_PublishCount.fetch_add(1, std::memory_order_relaxed);
        }

        /**
         * Reader: if the writer published a value that the reader has not seen, make it
         * the read slot and return true. Otherwise keep the read slot and return false.
         */
        bool AcquireLatest()
        {
            if ((m_Ready.load(std::memory_order_relaxed) & NewBit) == 0)
            {
                return false;
            }
            const uint8 previous = m_Ready.exchange(m_ReadIndex, std::memory_order_acq_rel);
            m_ReadIndex = static_cast<uint8>(previous & IndexMask);
            return true;
        }

        /** Reader: the value acquired last. Before the first AcquireLatest that returns true, a default value. */
        T& ReadSlot() { return m_Slots[m_ReadIndex]; }

        const T& ReadSlot() const { return m_Slots[m_ReadIndex]; }

        /** How many times Publish ran. Any thread, for statistics only. */
        uint64 PublishCount() const { return m_PublishCount.load(std::memory_order_relaxed); }

    private:
        static constexpr uint8 IndexMask = 3;
        /** Set in m_Ready when the ready slot holds a value that the reader has not acquired. */
        static constexpr uint8 NewBit = 4;

        std::array<T, 3> m_Slots{};

        /** The index of the ready slot, plus NewBit. The only state both threads touch. */
        alignas(64) std::atomic<uint8> m_Ready{1};

        /** Only the writer uses it. */
        alignas(64) uint8 m_WriteIndex = 0;
        std::atomic<uint64> m_PublishCount{0};

        /** Only the reader uses it. */
        alignas(64) uint8 m_ReadIndex = 2;
    };
} // namespace Fling
