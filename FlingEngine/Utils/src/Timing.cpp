#include "Timing.h"

namespace Fling
{
	std::chrono::high_resolution_clock::time_point sStartTime;

	void Timing::Init()
	{
		sStartTime = std::chrono::high_resolution_clock::now();
	}

	void Timing::Update()
	{
		double currentTime = GetTime();

		m_deltaTime = (float)( currentTime - m_lastFrameStartTime );

		m_lastFrameStartTime = currentTime;
		m_frameStartTimef = static_cast<float> ( m_lastFrameStartTime );

		UpdateFps();
	}

	void Timing::UpdateFps()
	{
		m_fpsFrameCount++;

		float timeDiff = GetTimeSinceStart() - m_fpsTimeElapsed;
		if (timeDiff < 1.0f)
			return;

		//F_LOG_TRACE("FPS {} FRAMETIME {}", m_fpsFrameCount, mspf);

		mspf = 1000.0f / static_cast<float>(m_fpsFrameCount);

		m_fpsFrameCount = 0;
		m_fpsTimeElapsed += 1.0f;
	}

	double Timing::GetTime() const
	{
		auto now = std::chrono::high_resolution_clock::now();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>( now - sStartTime ).count();
		//a little uncool to then convert into a double just to go back, but oh well.
		return static_cast<double>( ms ) / 1000;
	}
}	// namespace Fling