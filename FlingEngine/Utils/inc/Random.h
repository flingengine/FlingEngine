#pragma once

#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

namespace Fling
{
	namespace Utils
	{

		class Random
		{
		public:

			/// <summary>
			/// Initialize the random number generator
			/// </summary>
			/// <returns>True if successful</returns>
			static bool Init();

			/// <summary>
			/// Get a random int between 0 and the max
			/// </summary>
			/// <param name="t_max">Max number to generate to</param>
			/// <returns>Number between 0 and the given</returns>
			static const int Random0ToN( const int t_max );

			/// <summary>
			/// Generate a random number between the two given values
			/// </summary>
			/// <param name="t_min">Min number to gerneate between</param>
			/// <param name="t_max">Max number to generate between</param>
			/// <returns>Random int between the two values</returns>
			static const int RandomBetween( const int t_min, const int t_max );

			/** Flag to determine if random has been initialized */
			static bool bIsInitalized;

		};

	};	// namespace Utils

}	// namespace Fling