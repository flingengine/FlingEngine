#pragma once

/** A mover will move between two values ata  given speed */
struct Mover
{
    glm::vec3 TargetPos = { -10.0f, 0.0f, 0.0f };

    float Speed = 1.0f;
};

/** A mover will move between two values ata  given speed */
struct Rotator
{
	glm::vec3 MinPos = { -10.0f, 0.0f, 0.0f };
	glm::vec3 MaxPos = { -10.0f, 0.0f, 0.0f } ;
	float Speed = 1.0f;
};