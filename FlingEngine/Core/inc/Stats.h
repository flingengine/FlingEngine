#pragma once

#include "MovingAverage.hpp"

namespace Fling
{
    class Engine;

    namespace Stats
    {
        struct Frames
        {
            friend class Fling::Engine;

        public:
            static float GetAverageFrameTime();

            static float GetAverageFPS();

            static void FrameStats(float t_DeltaTime);

        private:
            static MovingAverage<float, 100> FPSCounter;
        };
    } // namespace Stats
} // namespace Fling