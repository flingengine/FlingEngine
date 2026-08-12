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

		/**
		* Update the time values of the timer. Should be done at the beginning
		* of every frame
		*/
		void Update();

		/**
		* Updates current frame timer
		*/
		void UpdateFps();

		float FLING_API GetDeltaTime();

		/**
		 * Get the current time of the application (double)
		 */
		double FLING_API GetTime() const;

		/**
		 * Get the current time of the application (float)
		 */
		float FLING_API GetTimef() const
		{
			return static_cast<float>( GetTime() );
		}

		/**
		 * Get the time that that frame has started
		 */
		float FLING_API GetFrameStartTime() const { return m_frameStartTimef; }

		/**
		 * Get the time that the application has started
		 */
		double FLING_API GetStartTime() const { return m_startTime; }

		/**
		 * Get the time since that application has started (i.e. time running)
		 */
		float FLING_API GetTimeSinceStart() const { return GetTimef() - static_cast<float>(m_startTime); }

		/**
		 * Get fps count
		 */
		int FLING_API GetFrameCount() const { return m_fpsFrameCount; }

		/**
		 * Get current frame time
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