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
		 * Get a random int in [0, t_max): inclusive of 0, exclusive of t_max.
		 *
		 * @param t_max Exclusive upper bound; must be greater than 0
		 */
		static const int Random0ToN( const int t_max );

		/**
		 * Generate a random int in [t_min, t_max]: inclusive of both bounds.
		 *
		 * @param t_min Inclusive lower bound
		 * @param t_max Inclusive upper bound
		 */
		static const int RandomBetween( const int t_min, const int t_max );

		/** Flag to determine if random has been initialized */
		static bool bIsInitalized;

		/**
		 * Generate a random vec3 with each component independently sampled
		 * from [t_Min, t_Max): inclusive of t_Min, exclusive of t_Max.
		 */
		static glm::vec3 GetRandomVec3(const glm::vec3 t_Min, const glm::vec3 t_Max);

		/**
		 * Generate a random float in [t_Min, t_Max): inclusive of t_Min,
		 * exclusive of t_Max.
		 */
		static float GetRandomFloat(float t_Min, float t_Max);

	private:

		/** Engine backing all Random calls; owned per-process, not shared libc state */
		static std::mt19937 s_Engine;
	};
}	// namespace Fling
