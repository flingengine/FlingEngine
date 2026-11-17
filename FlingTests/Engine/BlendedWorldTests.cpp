#include "Components/Transform.h"
#include "Ecs/Entity.h"
#include "Interpolate.h"
#include "TransformUtil.h"

#include <catch2/catch_all.hpp>
#include <map>
#include <vector>

// Interpolate::BlendedWorld walks the parent chain once, with a fixed table for the first
// levels, and falls back to BlendedWorldDeep (two walks, any depth) for a longer chain. These
// tests hold the two to the same answer at every depth around that limit, and
// TransformUtil::LocalMatrix to the glm products it replaced.

using namespace Fling;

namespace
{
    /** The calls of a frame system context that BlendedWorld uses, over plain maps. */
    struct FakeContext
    {
        std::map<uint64, Transform> From;
        std::map<uint64, Transform> To;
        std::map<uint64, Transform> Latest;
        std::map<uint64, Parent> Parents;

        template <typename T>
        const T* TryGet(const Entity t_Entity) const;

        template <typename T>
        const Transform& BlendFrom(const Entity t_Entity) const
        {
            return From.at(t_Entity.ToBits());
        }

        template <typename T>
        const Transform& BlendTo(const Entity t_Entity) const
        {
            return To.at(t_Entity.ToBits());
        }
    };

    template <>
    const Transform* FakeContext::TryGet<const Transform>(const Entity t_Entity) const
    {
        const auto It = Latest.find(t_Entity.ToBits());
        return It == Latest.end() ? nullptr : &It->second;
    }

    template <>
    const Parent* FakeContext::TryGet<const Parent>(const Entity t_Entity) const
    {
        const auto It = Parents.find(t_Entity.ToBits());
        return It == Parents.end() ? nullptr : &It->second;
    }

    Transform MakeTransform(const float t_Seed)
    {
        Transform T;
        T.Position = glm::vec3(t_Seed, t_Seed * 0.5f + 1.0f, -t_Seed);
        T.Rotation = glm::angleAxis(glm::radians(10.0f * t_Seed), glm::normalize(glm::vec3(1.0f, 2.0f, 0.5f)));
        T.Scale = glm::vec3(1.0f + 0.1f * t_Seed, 1.0f, 2.0f);
        return T;
    }

    /**
     * A chain of t_Depth entities, the first is the child, the last has no parent. The level
     * t_MovedLevel changes between the two ticks (-1: none moved).
     */
    Entity BuildChain(FakeContext& t_Ctx, const uint32 t_Depth, const int t_MovedLevel)
    {
        for (uint32 Level = 0; Level < t_Depth; ++Level)
        {
            const Entity E{Level, 0};
            const Transform Base = MakeTransform(static_cast<float>(Level) + 1.0f);
            Transform To = Base;
            if (static_cast<int>(Level) == t_MovedLevel)
            {
                To = MakeTransform(static_cast<float>(Level) + 1.7f);
            }
            t_Ctx.From[E.ToBits()] = Base;
            t_Ctx.To[E.ToBits()] = To;
            t_Ctx.Latest[E.ToBits()] = To;
            if (Level + 1 < t_Depth)
            {
                t_Ctx.Parents[E.ToBits()] = Parent{Entity{Level + 1, 0}};
            }
        }
        return Entity{0, 0};
    }
} // namespace

TEST_CASE("BlendedWorld gives what the two walk version gives, at every depth around its limit", "[interpolate]")
{
    for (const uint32 Depth : {1u, 2u, 4u, 7u, 8u, 9u, 12u})
    {
        for (int Moved = -1; Moved < static_cast<int>(Depth); ++Moved)
        {
            for (const float Alpha : {0.0f, 0.25f, 0.8f, 1.0f})
            {
                FakeContext Ctx;
                const Entity Child = BuildChain(Ctx, Depth, Moved);

                glm::mat4 Fast{-1.0f};
                glm::mat4 Reference{-1.0f};
                const bool FastMoved = Interpolate::BlendedWorld(Ctx, Child, Alpha, Fast);
                const bool ReferenceMoved = Interpolate::BlendedWorldDeep(Ctx, Child, Alpha, Reference);

                INFO("depth " << Depth << " moved level " << Moved << " alpha " << Alpha);
                REQUIRE(FastMoved == ReferenceMoved);
                REQUIRE(FastMoved == (Moved >= 0));
                if (FastMoved)
                {
                    REQUIRE(Fast == Reference);
                }
                else
                {
                    // Nothing moved: the output is left alone
                    REQUIRE(Fast == glm::mat4{-1.0f});
                }
            }
        }
    }
}

TEST_CASE("BlendedWorld ends the chain at a parent that has no Transform", "[interpolate]")
{
    FakeContext Ctx;
    const Entity Child = BuildChain(Ctx, 3, 0);
    // Take the top of the chain away: the child's Parent still points at it
    Ctx.Latest.erase(Entity{2, 0}.ToBits());

    glm::mat4 Fast{1.0f};
    glm::mat4 Reference{1.0f};
    REQUIRE(Interpolate::BlendedWorld(Ctx, Child, 0.5f, Fast));
    REQUIRE(Interpolate::BlendedWorldDeep(Ctx, Child, 0.5f, Reference));
    REQUIRE(Fast == Reference);
}

TEST_CASE("LocalMatrix is the translate, rotate, scale product", "[transform]")
{
    for (float Seed = -3.0f; Seed < 4.0f; Seed += 0.7f)
    {
        const Transform T = MakeTransform(Seed);
        glm::mat4 Reference = glm::translate(glm::mat4(1.0f), T.Position);
        Reference = Reference * glm::mat4_cast(T.Rotation);
        Reference = glm::scale(Reference, T.Scale);
        // == on a matrix compares each value, so a -0 equals a 0
        REQUIRE(TransformUtil::LocalMatrix(T) == Reference);
    }
}
