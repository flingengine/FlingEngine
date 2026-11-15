#pragma once

#include "Components/Transform.h"
#include "Ecs/Entity.h"
#include "FlingMath.h"
#include "FlingTypes.h"
#include "TransformUtil.h"

/**
 * Blends for a frame system that draws between the latest two published ticks
 * (Docs/threading-model.md, "The two blend states"). Declare Read<T> and ReadBlend<T>, read
 * the values at both ends, and blend them by the frame's RealFrameTime::BlendAlpha:
 *
 *     const float alpha = ctx.Singleton<const Fling::RealFrameTime>().BlendAlpha;
 *     for (auto [entity, spinner] : ctx.Query<const Spinner>())
 *     {
 *         const float angle = Fling::Interpolate::Lerp(
 *             ctx.BlendFrom<Spinner>(entity).Angle, ctx.BlendTo<Spinner>(entity).Angle, alpha);
 *     }
 *
 * Each blend gives exactly the first value at 0 and exactly the second at 1. They are pure
 * functions of their arguments; the blend's state (the two ticks and the point drawn between
 * them) belongs to FrameBlend.
 */
namespace Fling::Interpolate
{
    /** a at t <= 0, b at t >= 1, the straight line between them otherwise. */
    inline float Lerp(const float a, const float b, const float t)
    {
        if (t <= 0.0f)
        {
            return a;
        }
        if (t >= 1.0f)
        {
            return b;
        }
        return a + (b - a) * t;
    }

    /** a at t <= 0, b at t >= 1, the straight line between them otherwise. */
    inline glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, const float t)
    {
        if (t <= 0.0f)
        {
            return a;
        }
        if (t >= 1.0f)
        {
            return b;
        }
        return a + (b - a) * t;
    }

    /**
     * The rotation a fraction t of the way from a to b, by the shorter way around: q and -q
     * are the same rotation, so b is first taken to a's side. Constant angular speed in t.
     */
    inline glm::quat Slerp(const glm::quat& a, const glm::quat& b, const float t)
    {
        if (t <= 0.0f)
        {
            return a;
        }
        if (t >= 1.0f)
        {
            return b;
        }
        const glm::quat sameSide = glm::dot(a, b) < 0.0f ? -b : b;
        return glm::normalize(glm::slerp(a, sameSide, t));
    }

    /**
     * Position and scale by Lerp, rotation by Slerp. Blend the transforms and then make the
     * matrix: a blend of two matrices is not a rotation (it shrinks and shears).
     */
    inline Transform Blend(const Transform& from, const Transform& to, const float t)
    {
        Transform out;
        out.Position = Lerp(from.Position, to.Position, t);
        out.Rotation = Slerp(from.Rotation, to.Rotation, t);
        out.Scale = Lerp(from.Scale, to.Scale, t);
        return out;
    }

    /** True if the two transforms have the same values (then nothing moved between the two ticks). */
    inline bool Same(const Transform& a, const Transform& b)
    {
        return a.Position == b.Position && a.Rotation == b.Rotation && a.Scale == b.Scale;
    }

    /**
     * Calls fn(from, to, latest) for the Transform of the entity and of each parent
     * above it (ctx.BlendFrom, ctx.BlendTo and the latest value), until fn returns false. The
     * chain ends at a parent that is gone or has no Transform, as in
     * TransformUtil::WorldFromParents.
     */
    template <typename Ctx, typename Fn>
    void ForEachUp(Ctx& t_Ctx, const Entity t_Entity, Fn&& t_Fn)
    {
        Entity At = t_Entity;
        // HierarchyUtil refuses loops, and the limit keeps bad data from hanging
        for (uint32 Depth = 0; Depth < 4096; ++Depth)
        {
            const Transform* Latest = t_Ctx.template TryGet<const Transform>(At);
            if (Latest == nullptr ||
                !t_Fn(t_Ctx.template BlendFrom<Transform>(At), t_Ctx.template BlendTo<Transform>(At), *Latest))
            {
                return;
            }
            const Parent* Up = t_Ctx.template TryGet<const Parent>(At);
            if (Up == nullptr)
            {
                return;
            }
            At = Up->Value;
        }
    }

    /**
     * The world matrix of an entity between the two ticks of the frame's blend (ctx.BlendFrom
     * and ctx.BlendTo): each Transform up its Parent chain is blended at t, then the matrices
     * are composed. So a child of a rotating parent moves on the arc and stays rigid; a
     * blend of world matrices or world positions would cut the chord.
     *
     * Returns false, and leaves out alone, when no Transform on the chain differs between
     * the two ticks and the latest one: then the published LocalToWorld is the exact matrix,
     * and the check is all it costs. t_Ctx is a frame system context with Read<Transform>,
     * ReadBlend<Transform> and Read<Parent>.
     */
    template <typename Ctx>
    bool BlendedWorld(Ctx& t_Ctx, const Entity t_Entity, const float t, glm::mat4& out)
    {
        bool Moved = false;
        ForEachUp(t_Ctx,
                  t_Entity,
                  [&Moved](const Transform& t_From, const Transform& t_To, const Transform& t_Latest)
                  {
                      Moved = !Same(t_From, t_To) || !Same(t_To, t_Latest);
                      return !Moved;
                  });
        if (!Moved)
        {
            return false;
        }
        glm::mat4 World{1.0f};
        ForEachUp(t_Ctx,
                  t_Entity,
                  [&World, t](const Transform& t_From, const Transform& t_To, const Transform&)
                  {
                      World = TransformUtil::LocalMatrix(Blend(t_From, t_To, t)) * World;
                      return true;
                  });
        out = World;
        return true;
    }
} // namespace Fling::Interpolate
