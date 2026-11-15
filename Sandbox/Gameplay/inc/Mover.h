#pragma once

#include "Components/Transform.h"
#include "Ecs/Component.h"
#include "FlingMath.h"

#include <cstddef>

/** A mover will move between two values ata  given speed */
struct Mover
{
    static constexpr Fling::ComponentInfo Component{.Name = "Mover"};
    using Requires = Fling::TypeList<Fling::Transform>;

    glm::vec3 TargetPos = {-10.0f, 0.0f, 0.0f};

    float Speed = 1.0f;
};
// The same layout on every platform: a build where this fails has drifted from the others
static_assert(sizeof(Mover) == 16 && offsetof(Mover, Speed) == 12);

/** A mover will move between two values ata  given speed */
struct Rotator
{
    static constexpr Fling::ComponentInfo Component{.Name = "Rotator"};
    using Requires = Fling::TypeList<Fling::Transform>;

    glm::vec3 MinPos = {-10.0f, 0.0f, 0.0f};
    glm::vec3 MaxPos = {-10.0f, 0.0f, 0.0f};
    float Speed = 1.0f;
};
// The same layout on every platform: a build where this fails has drifted from the others
static_assert(sizeof(Rotator) == 28 && offsetof(Rotator, MaxPos) == 12 && offsetof(Rotator, Speed) == 24);
