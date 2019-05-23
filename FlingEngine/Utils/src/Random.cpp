#include "pch.h"
#include "Random.h"

namespace Utils
{
	bool Random::bIsInitalized = false;

	bool Random::Init()
	{
		/* initialize random seed */
		srand( static_cast<unsigned int>( time( NULL ) ) );
		
		bIsInitalized = true;
		return bIsInitalized;
	}

	const int Random::Random0ToN( const int t_max )
	{
		assert( t_max > 0 && bIsInitalized );
		// value from 0 to max;
		return rand() % t_max;
	}

	const int Random::RandomBetween( const int t_min, const int t_max )
	{
		assert( bIsInitalized );
		// Range from minx to max
		return rand() % t_max + t_min;
	}
}