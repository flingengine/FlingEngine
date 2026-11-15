#include "Stats.h"
#include "Threading/Lane.h"

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

        void Frames::FrameStats(float t_DeltaTime)
        {
            // Frame statistics: the main thread's frame loop owns them
            FLING_LANE_CHECK(Lane::Frame);
            FPSCounter.Push(t_DeltaTime);
        }
    } // namespace Stats
} // namespace Fling