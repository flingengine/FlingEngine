#pragma once

#include "Components/Transform.h"
#include "FlingMath.h"

namespace Fling::TransformUtil
{
    /** The matrix of the transform alone: translate, then rotate, then scale. */
    glm::mat4 LocalMatrix(const Transform& t_Transform);

    /**
     * A rotation from Euler angles in degrees, applied as yaw (Y), then pitch (X),
     * then roll (Z). The same order the engine used when Transform stored Euler angles.
     */
    glm::quat FromEulerDegrees(const glm::vec3& t_Degrees);

    /** Euler angles in degrees (X pitch, Y yaw, Z roll) for a rotation. For display and editing. */
    glm::vec3 ToEulerDegrees(const glm::quat& t_Rotation);

    /** Rotate by t_Degrees around a world axis, after the current rotation. */
    void RotateAround(Transform& t_Transform, const glm::vec3& t_Axis, float t_Degrees);

    /** The direction the transform faces (-Z), right (+X), and up (+Y), in its parent's space. */
    glm::vec3 Forward(const Transform& t_Transform);
    glm::vec3 Right(const Transform& t_Transform);
    glm::vec3 Up(const Transform& t_Transform);

    /** The translation part of a world matrix. */
    inline glm::vec3 WorldPosition(const LocalToWorld& t_World)
    {
        return glm::vec3(t_World.Value[3]);
    }
} // namespace Fling::TransformUtil
