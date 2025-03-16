#pragma once

#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include "FlingMath.h"

namespace Fling
{
	class FLING_API Random
	{
	public:

		/**
		* Initialize the random number generator
		*
		* @return True if successful
		*/ 
		static bool Init();

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
	};
}	// namespace Fling