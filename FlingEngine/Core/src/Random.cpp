#include "pch.h"
#include "Random.h"

namespace Fling
{
	bool Random::bIsInitalized = false;
	std::mt19937 Random::s_Engine;

	bool Random::Init()
	{
		// Seed once from a real entropy source; every call afterwards just
		// advances the mt19937 state, which is cheap (no per-call syscalls).
		std::random_device seedSource;
		return Init( seedSource() );
	}

	bool Random::Init( uint32 t_Seed )
	{
		s_Engine.seed( t_Seed );

		bIsInitalized = true;
		return bIsInitalized;
	}

	const int Random::Random0ToN( const int t_max )
	{
		assert( t_max > 0 && bIsInitalized );
		// value from 0 to max;
		std::uniform_int_distribution<int> dist( 0, t_max - 1 );
		return dist( s_Engine );
	}

	const int Random::RandomBetween( const int t_min, const int t_max )
	{
		assert( bIsInitalized );
		// Range from min to max, inclusive
		std::uniform_int_distribution<int> dist( t_min, t_max );
		return dist( s_Engine );
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
		std::uniform_real_distribution<float> dist( t_Min, t_Max );
		return dist( s_Engine );
	}
}	// namespace Fling
