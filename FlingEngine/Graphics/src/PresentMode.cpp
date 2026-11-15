#include "PresentMode.h"

#include <algorithm>

namespace Fling
{
    const char* PresentModeName(const PresentMode t_Mode)
    {
        switch (t_Mode)
        {
        case PresentMode::Immediate:
            return "IMMEDIATE";
        case PresentMode::Mailbox:
            return "MAILBOX";
        case PresentMode::Fifo:
            return "FIFO";
        case PresentMode::FifoRelaxed:
            return "FIFO_RELAXED";
        }
        return "UNKNOWN";
    }

    PresentMode ChoosePresentMode(const std::span<const PresentMode> t_Available, const bool t_VSync)
    {
        if (!t_VSync)
        {
            for (const PresentMode NoWait : {PresentMode::Immediate, PresentMode::Mailbox})
            {
                if (std::ranges::find(t_Available, NoWait) != t_Available.end())
                {
                    return NoWait;
                }
            }
        }
        return PresentMode::Fifo;
    }

    bool DisplayPacesFrames(const PresentMode t_Mode)
    {
        return t_Mode == PresentMode::Fifo || t_Mode == PresentMode::FifoRelaxed;
    }
} // namespace Fling
