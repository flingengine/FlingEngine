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

    /**
     * The world matrix of an entity from its Transform and the Parent links above it,
     * with no use of LocalToWorld. Slow: it walks to the root. Engine.Transform keeps
     * LocalToWorld for normal use. This is for an entity that has no LocalToWorld yet
     * (it was made after the last tick), and for the system itself.
     * t_Ctx is a system context that can read Transform and Parent. t_Parent is the
     * entity's parent, or Entity::Null.
     */
    template <typename Ctx>
    glm::mat4 WorldFromParents(Ctx& t_Ctx, const Transform& t_Local, const Entity t_Parent)
    {
        glm::mat4 World = LocalMatrix(t_Local);
        Entity Up = t_Parent;
        // HierarchyUtil refuses loops, and the limit keeps bad data from hanging
        for (uint32 Depth = 0; Depth < 4096; ++Depth)
        {
            // A parent that is dead, or has no Transform, is not a parent: the chain ends there
            const Transform* Above = t_Ctx.template TryGet<const Transform>(Up);
            if (Above == nullptr)
            {
                break;
            }
            World = LocalMatrix(*Above) * World;
            const Parent* Next = t_Ctx.template TryGet<const Parent>(Up);
            if (Next == nullptr)
            {
                break;
            }
            Up = Next->Value;
        }
        return World;
    }
} // namespace Fling::TransformUtil
