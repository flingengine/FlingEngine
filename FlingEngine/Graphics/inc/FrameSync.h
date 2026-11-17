#pragma once

#include "FlingTypes.h"

#include <utility>
#include <vector>

namespace Fling
{
    /**
     * Which frame the renderer records now, and which frames the GPU has finished.
     *
     * The renderer keeps VkConfig::MAX_FRAMES_IN_FLIGHT frames in flight: the CPU records frame N while
     * the GPU still draws frame N-1. VulkanApp::Update calls Begin after it waited for the fence of the
     * frame slot, so every frame up to N - FramesInFlight has finished on the GPU, and the frame N - 1 may
     * still run. A resource that a recorded frame used can go only when IsDone says so.
     *
     * Main (render) thread only.
     */
    class FrameSync
    {
    public:
        /** How many frames the CPU can record ahead of the GPU. VulkanApp.cpp checks that it equals
         * VkConfig::MAX_FRAMES_IN_FLIGHT. */
        static constexpr uint32 FramesInFlight = 2;

        /** Start frame t_Frame. Call it after the fence of the frame slot is done. */
        static void Begin(const uint64 t_Frame) { s_Frame = t_Frame; }

        /** The number of the frame that is recorded now */
        static uint64 Frame() { return s_Frame; }

        /** The frame slot: the index of the resources that only one frame in flight uses */
        static uint32 Slot() { return static_cast<uint32>(s_Frame % FramesInFlight); }

        /** True if frame t_Frame and all before it are finished on the GPU, so what they used can go */
        static bool IsDone(const uint64 t_Frame)
        {
            return t_Frame + FramesInFlight <= s_Frame || t_Frame < s_IdleBefore;
        }

        /** The device was idle just now: every frame before the current one is done. */
        static void MarkIdle() { s_IdleBefore = s_Frame; }

        /** Start again at frame 0 (a new renderer, a test) */
        static void Reset()
        {
            s_Frame = 0;
            s_IdleBefore = 0;
        }

    private:
        static inline uint64 s_Frame = 0;
        static inline uint64 s_IdleBefore = 0;
    };

    /**
     * Things that were used by a frame in flight and can be freed only when the GPU is done with that
     * frame. Add tags the item with the current frame; Release gives back the ones that are done.
     */
    template <typename T>
    class RetireList
    {
    public:
        void Add(T t_Item) { m_Items.emplace_back(FrameSync::Frame(), std::move(t_Item)); }

        /** Call t_Free for each item whose frame is done, and forget it */
        template <typename Fn>
        void Release(Fn&& t_Free)
        {
            size_t Kept = 0;
            for (size_t i = 0; i < m_Items.size(); ++i)
            {
                if (FrameSync::IsDone(m_Items[i].first))
                {
                    t_Free(m_Items[i].second);
                }
                else
                {
                    if (Kept != i)
                    {
                        m_Items[Kept] = std::move(m_Items[i]);
                    }
                    ++Kept;
                }
            }
            m_Items.resize(Kept);
        }

        /** Call t_Free for every item, done or not. The device is idle. */
        template <typename Fn>
        void ReleaseAll(Fn&& t_Free)
        {
            for (auto& Item : m_Items)
            {
                t_Free(Item.second);
            }
            m_Items.clear();
        }

        bool Empty() const { return m_Items.empty(); }

    private:
        std::vector<std::pair<uint64, T>> m_Items;
    };
} // namespace Fling
