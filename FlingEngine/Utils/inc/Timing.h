#pragma once

// A basic timing class that was taken from: this multiplayer book:
// https://github.com/BenjaFriend/MultiplayerBook/blob/master/Chapter%208/RoboCatAction/RoboCat/Src/Timing.cpp

#include "pch.h"
#include <chrono>

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

		float FLING_API GetDeltaTime() const { return m_deltaTime; }

		/**
		 * @brief Get the current time of the application (double)
		 * 
		 * @return double GetTime 
		 */
		double FLING_API GetTime() const;

		/**
		 * @brief Get the current time of the application (float)
		 * 
		 * @return float GetTimef 
		 */
		float FLING_API GetTimef() const
		{
			return static_cast<float>( GetTime() );
		}

		/**
		 * @brief Get the time that that frame has started
		 * 
		 * @return float GetFrameStartTime 
		 */
		float FLING_API GetFrameStartTime() const { return m_frameStartTimef; }

		/**
		 * @brief Get the time that the application has started
		 * 
		 * @return double GetStartTime 
		 */
		double FLING_API GetStartTime() const { return m_startTime; }

		/**
		 * @brief Get the time since that application has started (i.e. time running)
		 * 
		 * @return float GetTimeSinceStart 
		 */
		float FLING_API GetTimeSinceStart() const { return GetTimef() - static_cast<float>(m_startTime); }

	private:

		float m_deltaTime = 0.0f;

		double m_lastFrameStartTime = 0.0;
		float m_frameStartTimef = 0.0f;

		/** The time that the program started */
		double m_startTime = 0.0;

	};
}	// namespace Fling