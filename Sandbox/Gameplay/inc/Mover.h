#pragma once

/** A mover will move between two values ata  given speed */
struct Mover
{
    float MinPos = -10.0f;
    float MaxPos = 10.0f;
    float Speed = 1.0f;
};

/** A mover will move between two values ata  given speed */
struct Rotator
{
	float MinPos = -10.0f;
	float MaxPos = 10.0f;
	float Speed = 1.0f;
};