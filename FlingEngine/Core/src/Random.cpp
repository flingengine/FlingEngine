#include "pch.h"
#include "Random.h"

namespace Fling
{
	namespace
	{
		uint32 GenerateEntropySeed()
		{
			std::random_device seedSource;
			return seedSource();
		}

		int Random0ToNImpl(std::mt19937& t_Engine, const int t_max)
		{
			assert(t_max > 0);
			// value from 0 to max;
			std::uniform_int_distribution<int> dist(0, t_max - 1);
			return dist(t_Engine);
		}

		int RandomBetweenImpl(std::mt19937& t_Engine, const int t_min, const int t_max)
		{
			// Range from min to max, inclusive
			std::uniform_int_distribution<int> dist(t_min, t_max);
			return dist(t_Engine);
		}

		float GetRandomFloatImpl(std::mt19937& t_Engine, float t_Min, float t_Max)
		{
			std::uniform_real_distribution<float> dist(t_Min, t_Max);
			return dist(t_Engine);
		}

		glm::vec3 GetRandomVec3Impl(std::mt19937& t_Engine, const glm::vec3 t_Min, const glm::vec3 t_Max)
		{
			glm::vec3 rand = {};
			rand.x = GetRandomFloatImpl(t_Engine, t_Min.x, t_Max.x);
			rand.y = GetRandomFloatImpl(t_Engine, t_Min.y, t_Max.y);
			rand.z = GetRandomFloatImpl(t_Engine, t_Min.z, t_Max.z);
			return rand;
		}
	}	// namespace

	RandomStream::RandomStream()
		: m_Engine(GenerateEntropySeed())
	{
	}

	RandomStream::RandomStream(uint32 t_Seed)
		: m_Engine(t_Seed)
	{
	}

	void RandomStream::Reseed()
	{
		m_Engine.seed(GenerateEntropySeed());
	}

	void RandomStream::Reseed(uint32 t_Seed)
	{
		m_Engine.seed(t_Seed);
	}

	int RandomStream::Random0ToN(const int t_max)
	{
		return Random0ToNImpl(m_Engine, t_max);
	}

	int RandomStream::RandomBetween(const int t_min, const int t_max)
	{
		return RandomBetweenImpl(m_Engine, t_min, t_max);
	}

	float RandomStream::GetRandomFloat(float t_Min, float t_Max)
	{
		return GetRandomFloatImpl(m_Engine, t_Min, t_Max);
	}

	glm::vec3 RandomStream::GetRandomVec3(const glm::vec3 t_Min, const glm::vec3 t_Max)
	{
		return GetRandomVec3Impl(m_Engine, t_Min, t_Max);
	}

	bool Random::bIsInitalized = false;
	RandomStream Random::s_Stream;

	bool Random::Init()
	{
		// Seed once from a real entropy source; every call afterwards just
		// advances the mt19937 state, which is cheap (no per-call syscalls).
		return Init(GenerateEntropySeed());
	}

	bool Random::Init(uint32 t_Seed)
	{
		s_Stream.Reseed(t_Seed);

		bIsInitalized = true;
		return bIsInitalized;
	}

	const int Random::Random0ToN(const int t_max)
	{
		assert(bIsInitalized);
		return s_Stream.Random0ToN(t_max);
	}

	const int Random::RandomBetween(const int t_min, const int t_max)
	{
		assert(bIsInitalized);
		return s_Stream.RandomBetween(t_min, t_max);
	}

	glm::vec3 Random::GetRandomVec3(const glm::vec3 t_Min, const glm::vec3 t_Max)
	{
		return s_Stream.GetRandomVec3(t_Min, t_Max);
	}

	float Random::GetRandomFloat(float t_Min, float t_Max)
	{
		return s_Stream.GetRandomFloat(t_Min, t_Max);
	}
}	// namespace Fling
