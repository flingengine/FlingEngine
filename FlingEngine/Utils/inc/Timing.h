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

		/// <summary>
		/// Updates current frame timer
		/// </summary>
		void UpdateFps();

		float FLING_API GetDeltaTime();

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

		/**
		 * @brief Get fps count
		 *
		 * @return int GetFrameCount
		 */
		int FLING_API GetFrameCount() const { return m_fpsFrameCount; }

		/**
		 * @brief Get current frame time
		 *
		 * @return float GetFrameTime
		 */
		float FLING_API GetFrameTime() const { return 1000.0f / static_cast<float>(m_fpsFrameCount); }

	private:

		// Initialize delta time at 60 FPS to avoid an ImGUI assertion
		float m_deltaTime = 1.0f / 60.0f;

		double m_lastFrameStartTime = 0.0;
		float m_frameStartTimef = 0.0f;

		float m_fpsTimeElapsed = 0.0f;
		int m_fpsFrameCount = 0;
		int m_fpsFrameCountTemp = 0;

		/** The time that the program started */
		double m_startTime = 0.0;
	};
}	// namespace Fling