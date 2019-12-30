#pragma once

#include "Serilization.h"

/** A mover will move between two values ata  given speed */
struct Mover
{
    glm::vec3 TargetPos = { -10.0f, 0.0f, 0.0f };

    float Speed = 1.0f;

	template<class Archive>
	void serialize(Archive& t_Archive);
};

/** Serilazation to an archive */
template<class Archive>
void Mover::serialize(Archive& t_Archive)
{
	t_Archive(
		cereal::make_nvp("TargetPos_X", TargetPos.x),
		cereal::make_nvp("TargetPos_Y", TargetPos.y),
		cereal::make_nvp("TargetPos_Z", TargetPos.z),

		cereal::make_nvp("Speed", Speed)
	);
}

/** A mover will move between two values ata  given speed */
struct Rotator
{
	glm::vec3 MinPos = { -10.0f, 0.0f, 0.0f };
	glm::vec3 MaxPos = { -10.0f, 0.0f, 0.0f } ;
	float Speed = 1.0f;


	template<class Archive>
	void serialize(Archive& t_Archive);
};

/** Serilazation to an archive */
template<class Archive>
void Rotator::serialize(Archive& t_Archive)
{
	t_Archive(
		cereal::make_nvp("MinPos_X", MinPos.x),
		cereal::make_nvp("MinPos_Y", MinPos.y),
		cereal::make_nvp("MinPos_Z", MinPos.z),

		cereal::make_nvp("MaxPos_X", MaxPos.x),
		cereal::make_nvp("MaxPos_Y", MaxPos.y),
		cereal::make_nvp("MaxPos_Z", MaxPos.z),

		cereal::make_nvp("Speed", Speed)
	);
}