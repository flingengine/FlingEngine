#include "PresentMode.h"
#include "Strings/String.h"

#include <catch2/catch_all.hpp>
#include <vector>

using namespace Fling;

TEST_CASE("Present mode: VSync on is FIFO on every surface", "[PresentMode]")
{
    using Modes = std::vector<PresentMode>;
    for (const Modes& Offered : {Modes{PresentMode::Fifo},
                                 Modes{PresentMode::Fifo, PresentMode::Mailbox, PresentMode::Immediate},
                                 Modes{PresentMode::FifoRelaxed, PresentMode::Fifo}})
    {
        REQUIRE(ChoosePresentMode(Offered, true) == PresentMode::Fifo);
    }
}

TEST_CASE("Present mode: VSync off is IMMEDIATE, else MAILBOX, else FIFO", "[PresentMode]")
{
    using Modes = std::vector<PresentMode>;
    REQUIRE(ChoosePresentMode(Modes{PresentMode::Fifo, PresentMode::Mailbox, PresentMode::Immediate}, false) ==
            PresentMode::Immediate);
    REQUIRE(ChoosePresentMode(Modes{PresentMode::Mailbox, PresentMode::Fifo}, false) == PresentMode::Mailbox);
    REQUIRE(ChoosePresentMode(Modes{PresentMode::Fifo, PresentMode::FifoRelaxed}, false) == PresentMode::Fifo);
    REQUIRE(ChoosePresentMode(Modes{}, false) == PresentMode::Fifo);
}

TEST_CASE("Present mode: the display paces the frames in FIFO and FIFO_RELAXED only", "[PresentMode]")
{
    REQUIRE(DisplayPacesFrames(PresentMode::Fifo));
    REQUIRE(DisplayPacesFrames(PresentMode::FifoRelaxed));
    REQUIRE_FALSE(DisplayPacesFrames(PresentMode::Mailbox));
    REQUIRE_FALSE(DisplayPacesFrames(PresentMode::Immediate));
    REQUIRE(StringView(PresentModeName(PresentMode::FifoRelaxed)) == "FIFO_RELAXED");
}
