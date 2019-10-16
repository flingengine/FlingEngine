#include "pch.h"
#include "Stats.h"

namespace Fling
{
    namespace Stats
    {
        MovingAverage<float, 100> Frames::FPSCounter = {};

        float Frames::GetAverageFrameTime()
        {
            return FPSCounter.GetAverage();
        }

        float Frames::GetAverageFPS()
        {
            return 1.0f / FPSCounter.GetAverage();
        }

        void Frames::TickStats(float t_DeltaTime)
        {
            FPSCounter.Push(t_DeltaTime);
        }
    }
}