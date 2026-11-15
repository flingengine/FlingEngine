#pragma once

#include "FlingTypes.h"
#include "GraphicsAPI.h"

#include <span>

namespace Fling
{
    // In Graphics, not Core: only the swap chain and the code that shows its mode use it.

    /**
     * How the swap chain shows a finished image. The values are those of VkPresentModeKHR,
     * so this header does not need Vulkan (SwapChain.cpp checks them).
     */
    enum class PresentMode : uint32
    {
        /** Shown at once; can tear. */
        Immediate = 0,

        /** The newest image waits for the next refresh and replaces an older waiting one; does not block. */
        Mailbox = 1,

        /** A queue shown one image per refresh. Every device has it. */
        Fifo = 2,

        /** As Fifo, but an image that missed its refresh is shown at once (can tear). */
        FifoRelaxed = 3,
    };

    /** "IMMEDIATE", "MAILBOX", "FIFO" or "FIFO_RELAXED" (the VK_PRESENT_MODE_..._KHR names). */
    GRAPHICS_API const char* PresentModeName(PresentMode t_Mode);

    /**
     * The present mode for the swap chain from the VSync setting and the modes the surface offers.
     *
     * - VSync on: Fifo (every surface offers it).
     * - VSync off: Immediate, else Mailbox, else Fifo. Frames do not wait for the display and
     *   may tear, for the lowest latency; the frame cap keeps the rate down.
     */
    GRAPHICS_API PresentMode ChoosePresentMode(std::span<const PresentMode> t_Available, bool t_VSync);

    /**
     * True for the modes where present waits for the display's refresh, so the display paces
     * the frames: Fifo and FifoRelaxed (RunControls::LimiterFps).
     */
    GRAPHICS_API bool DisplayPacesFrames(PresentMode t_Mode);
} // namespace Fling
