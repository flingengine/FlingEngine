#pragma once

#include "Ecs/Component.h"
#include "JsonArchive.h"

/** A mover will move between two values ata  given speed */
struct Mover
{
	static constexpr Fling::ComponentInfo Component{.Name = "Mover"};

	glm::vec3 TargetPos = { -10.0f, 0.0f, 0.0f };

	float Speed = 1.0f;

	void Serialize(Fling::JsonArchive& Ar)
	{
		Ar << Fling::MakeNVP("targetPos", TargetPos);
		Ar << Fling::MakeNVP("speed", Speed);
	}
};

/** A mover will move between two values ata  given speed */
struct Rotator
{
	static constexpr Fling::ComponentInfo Component{.Name = "Rotator"};

	glm::vec3 MinPos = { -10.0f, 0.0f, 0.0f };
	glm::vec3 MaxPos = { -10.0f, 0.0f, 0.0f };
	float Speed = 1.0f;

	void Serialize(Fling::JsonArchive& Ar)
	{
		Ar << Fling::MakeNVP("minPos", MinPos);
		Ar << Fling::MakeNVP("maxPos", MaxPos);
		Ar << Fling::MakeNVP("speed", Speed);
	}
};
