
#include "Random.h"

namespace Fling
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

	glm::vec3 Random::GetRandomVec3(const glm::vec3 t_Min, const glm::vec3 t_Max)
	{
		glm::vec3 rand = {};
		rand.x = GetRandomFloat(t_Min.x, t_Max.x);
		rand.y = GetRandomFloat(t_Min.y, t_Max.y);
		rand.z = GetRandomFloat(t_Min.z, t_Max.z);
		return rand;
	}

	float Random::GetRandomFloat(float t_Min, float t_Max)
	{
		return (static_cast<float>(rand()) / static_cast<float>(static_cast<float>(RAND_MAX) / (t_Max - t_Min))) + t_Min;
	}
}	// namespace Fling