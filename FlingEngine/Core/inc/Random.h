#pragma once

#include <random>
#include "FlingMath.h"
#include "FlingTypes.h"

namespace Fling
{
	class FLING_API Random
	{
	public:

		/**
		* Initialize the random number generator with a non-deterministic seed
		*
		* @return True if successful
		*/
		static bool Init();

		/**
		* Initialize the random number generator with an explicit seed. Two
		* engines seeded with the same value produce the same sequence of
		* results, which is useful for reproducing a run in tests or when
		* tracking down a gameplay bug.
		*
		* @param t_Seed Seed value to reproduce a given sequence of random values
		* @return True if successful
		*/
		static bool Init( uint32 t_Seed );

		/**
		* Get a random int between 0 and the max
		*
		* @param t_max number to generate to
		* @return Number between 0 and the given
		*/
		static const int Random0ToN( const int t_max );

		/**
		 * Generate a random number between the two given values
		 *
		 * @param t_min Min number to gerneate between
		 * @param t_max Max number to generate between
		 * @return Random int between the two values
		 */
		static const int RandomBetween( const int t_min, const int t_max );

		/** Flag to determine if random has been initialized */
		static bool bIsInitalized;

		static glm::vec3 GetRandomVec3(const glm::vec3 t_Min, const glm::vec3 t_Max);

		static float GetRandomFloat(float t_Min, float t_Max);

	private:

		/** Engine backing all Random calls; owned per-process, not shared libc state */
		static std::mt19937 s_Engine;
	};
}	// namespace Fling
