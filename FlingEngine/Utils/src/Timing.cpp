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

		// Calculate a fall back delta time in case the engine ever gets out of sync
		const static float FallbackDeltaTime = 1.0f / 60.0f;
		const static float MaxDeltaTime = 1.0f;

		// If delta time is greater than 1 second, simulate it as 1/60 FPS 
		// because we can assume that it is like that because of debugging
		if (m_deltaTime >= MaxDeltaTime)
		{
			m_deltaTime = FallbackDeltaTime;
		} 

		m_lastFrameStartTime = currentTime;
		m_frameStartTimef = static_cast<float> ( m_lastFrameStartTime );
	}

	float Timing::GetDeltaTime()
	{
		return m_deltaTime;	
	}

	void Timing::UpdateFps()
	{
		m_fpsFrameCountTemp++;
		if (std::floor(GetTimeSinceStart()) > std::floor(m_fpsTimeElapsed))
		{
			m_fpsFrameCount = m_fpsFrameCountTemp;
			m_fpsFrameCountTemp = 0;

		}
		m_fpsTimeElapsed = GetTimeSinceStart();
	}

	double Timing::GetTime() const
	{
		auto now = std::chrono::high_resolution_clock::now();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>( now - sStartTime ).count();
		//a little uncool to then convert into a double just to go back, but oh well.
		return static_cast<double>( ms ) / 1000;
	}
}	// namespace Fling