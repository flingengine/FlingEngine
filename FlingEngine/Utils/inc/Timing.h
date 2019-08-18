// A basic timing class that was taken from: this multiplayer book:
// https://github.com/BenjaFriend/MultiplayerBook/blob/master/Chapter%208/RoboCatAction/RoboCat/Src/Timing.cpp

#pragma once

#include <chrono>

#include "FlingTypes.h"
#include "Singleton.hpp"

namespace Fling
{
    // #TODO Have a "game time" and "real time" 
    // @see 8.5.4 in Game Engine arch
	class Timing : public Singleton<Timing>
	{
	public:

		virtual void Init() override;

		/// <summary>
		/// Update the time values of the timer. Should be done at the beginning
		/// of every frame
		/// </summary>
		void Update();

		float GetDeltaTime() const { return m_deltaTime; }

		double GetTime() const;

		/// <summary>
		/// Get float 
		/// </summary>
		/// <returns></returns>
		float GetTimef() const
		{
			return static_cast<float>( GetTime() );
		}

		float GetFrameStartTime() const { return m_frameStartTimef; }

		double GetStartTime() const { return m_startTime; }

		float GetTimeSinceStart() const { return GetTimef() - static_cast<float>(m_startTime); }

	private:

		float m_deltaTime = 0.0f;
		UINT64 m_deltaTick = 0;

		double m_lastFrameStartTime = 0.0;
		float m_frameStartTimef = 0.0f;
		double m_perfCountDuration = 0.0;

		/** The time that the program started */
		double m_startTime = 0.0;

	};
}	// namespace Fling