#pragma once

#include "FlingTypes.h"
#include "Strings/String.h"

namespace Fling
{
    /**
     * How the world runs one schedule during play (Docs/threading-model.md section 4). Edit mode is
     * always Direct. The per-tick ledger (input cut and state hash) is the same in all
     * three, because the simulation never reads what the frame side has. Play uses
     * DefaultExecMode unless [Engine] Exec or --exec sets another (RunSettings).
     */
    enum class ExecMode : uint8
    {
        /** One thread, one storage: the frame systems read the simulation storage. */
        Direct,

        /**
         * One thread, two storages: after its ticks the world publishes a TickPacket, and
         * the frame systems read a mirror that the newest packet filled. For A/B and tests.
         */
        Inline,

        /**
         * The fixed ticks run on a game thread, paced by the platform clock. The main
         * thread runs the frame systems on the mirror and renders.
         */
        Threaded,
    };

    /** The exec mode of play when neither the config file nor the command line sets one. */
    constexpr ExecMode DefaultExecMode = ExecMode::Threaded;

    /** "direct", "inline" or "threaded". */
    constexpr const char* ExecModeName(const ExecMode t_Mode)
    {
        switch (t_Mode)
        {
        case ExecMode::Inline:
            return "inline";
        case ExecMode::Threaded:
            return "threaded";
        case ExecMode::Direct:
        default:
            return "direct";
        }
    }

    /** Read "direct", "inline" or "threaded". Returns false, and t_Out unchanged, for any other text. */
    inline bool ParseExecMode(const StringView t_Text, ExecMode& t_Out)
    {
        for (const ExecMode Mode : {ExecMode::Direct, ExecMode::Inline, ExecMode::Threaded})
        {
            if (t_Text == ExecModeName(Mode))
            {
                t_Out = Mode;
                return true;
            }
        }
        return false;
    }
} // namespace Fling
