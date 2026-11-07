#pragma once

#include "Ecs/Component.h"
#include "FlingMath.h"

/** A mover will move between two values ata  given speed */
struct Mover
{
    static constexpr Fling::ComponentInfo Component{.Name = "Mover"};

    glm::vec3 TargetPos = {-10.0f, 0.0f, 0.0f};

    float Speed = 1.0f;
};

/** A mover will move between two values ata  given speed */
struct Rotator
{
    static constexpr Fling::ComponentInfo Component{.Name = "Rotator"};

    glm::vec3 MinPos = {-10.0f, 0.0f, 0.0f};
    glm::vec3 MaxPos = {-10.0f, 0.0f, 0.0f};
    float Speed = 1.0f;
};
