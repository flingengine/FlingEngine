#pragma once

#include <random>
#include "FlingMath.h"
#include "FlingTypes.h"

namespace Fling
{
	/**
	 * A RandomStream object is a random number generator.
	 *
	 * Each RandomStream object has its own random number generator. 
	 *
	 * Do not call the methods of one RandomStream object from more than one
	 * thread at the same time.
	 */
	class FLING_API RandomStream
	{
	public:

		/**
		 * This constructor makes a new RandomStream object. It seeds the
		 * object with a random value from the operating system.
		 */
		RandomStream();

		/**
		 * This constructor makes a new RandomStream object. It seeds the
		 * object with the value t_Seed.
		 *
		 * Two RandomStream objects with the same seed value give the same
		 * sequence of numbers.
		 *
		 * @param t_Seed Use this value to seed the random number generator.
		 */
		explicit RandomStream(uint32 t_Seed);

		/**
		 * This method gives the stream a new seed. It gets the seed from
		 * the operating system.
		 */
		void Reseed();

		/**
		 * This method gives the stream the seed value t_Seed. The stream
		 * then starts a new sequence of numbers.
		 *
		 * @param t_Seed Use this value to seed the random number generator.
		 */
		void Reseed(uint32 t_Seed);

		/**
		 * This method gives a random whole number. The number is 0 or
		 * more. The number is less than t_max.
		 *
		 * @param t_max t_max must be more than 0.
		 */
		int Random0ToN(const int t_max);

		/**
		 * This method gives a random whole number between t_min and
		 * t_max. The number can be equal to t_min. The number can be
		 * equal to t_max.
		 *
		 * @param t_min This is the lowest possible value.
		 * @param t_max This is the highest possible value.
		 */
		int RandomBetween(const int t_min, const int t_max);

		/**
		 * This method gives a random decimal number. The number is greater
		 * than or equal to t_Min. The number is less than t_Max.
		 */
		float GetRandomFloat(float t_Min, float t_Max);

		/**
		 * This method gives a random vec3 value. Each component of the
		 * vec3 value (x, y, z) is a separate random number. Each number is
		 * more than or equal to the related component of t_Min. Each
		 * number is less than the related component of t_Max.
		 */
		glm::vec3 GetRandomVec3(const glm::vec3 t_Min, const glm::vec3 t_Max);

	private:

		/**
		 * This is the random number engine for this stream. It is
		 * separate from the engine of every other RandomStream object. It
		 * is separate from the engine of the Random class.
		 */
		std::mt19937 m_Engine;
	};

	/**
	 * Random is a static class. It gives random numbers to the whole
	 * process. It uses one shared RandomStream object.
	 *
	 * Call methods of the Random class from one thread only. If you need
	 * random numbers on a worker thread, make your own RandomStream
	 * object.
	 */
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
		static bool Init(uint32 t_Seed);

		/**
		 * Get a random int in [0, t_max): inclusive of 0, exclusive of t_max.
		 *
		 * @param t_max Exclusive upper bound; must be greater than 0
		 */
		static const int Random0ToN(const int t_max);

		/**
		 * Generate a random int in [t_min, t_max]: inclusive of both bounds.
		 *
		 * @param t_min Inclusive lower bound
		 * @param t_max Inclusive upper bound
		 */
		static const int RandomBetween(const int t_min, const int t_max);

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

		/** This is the shared RandomStream object for the Random class. */
		static RandomStream s_Stream;
	};
}	// namespace Fling
