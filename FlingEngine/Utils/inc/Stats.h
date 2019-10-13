#pragma once

#include "MovingAverage.hpp"

namespace Fling
{
    class Engine;

    namespace Stats
    {
        struct Frames
        {
        friend class Engine;
        public:
            static float GetAverageFrameTime();

            static float GetAverageFPS();
        
        private:
            
            static void TickStats(float t_DeltaTime);

            static MovingAverage<float, 100> FPSCounter;
        };
    }
}