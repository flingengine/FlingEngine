#include "Frustum.h"

namespace Fling
{
    namespace
    {
        /** Row t_Row of a glm matrix (glm is column major: m[column][row]). */
        glm::vec4 Row(const glm::mat4& t_M, const int t_Row)
        {
            return glm::vec4(t_M[0][t_Row], t_M[1][t_Row], t_M[2][t_Row], t_M[3][t_Row]);
        }

        glm::vec4 Normalized(const glm::vec4& t_Plane)
        {
            const float Length = glm::length(glm::vec3(t_Plane));
            return Length > 0.0f ? t_Plane / Length : t_Plane;
        }
    } // namespace

    void Aabb::Add(const glm::vec3& t_Point)
    {
        // Only the empty box starts again. A box made invalid by a NaN stays invalid, so it is never culled
        if (Min.x > Max.x)
        {
            Min = t_Point;
            Max = t_Point;
            return;
        }
        Min = glm::min(Min, t_Point);
        Max = glm::max(Max, t_Point);
    }

    Aabb Aabb::Of(const std::span<const glm::vec3> t_Points)
    {
        Aabb Box;
        for (const glm::vec3& Point : t_Points)
        {
            Box.Add(Point);
        }
        return Box;
    }

    Frustum Frustum::FromViewProjection(const glm::mat4& t_ViewProjection)
    {
        // A world point p is in the volume when its clip coordinates (row i . p) keep
        // -w <= x <= w, -w <= y <= w and 0 <= z <= w (Gribb and Hartmann, with Vulkan's depth)
        const glm::vec4 X = Row(t_ViewProjection, 0);
        const glm::vec4 Y = Row(t_ViewProjection, 1);
        const glm::vec4 Z = Row(t_ViewProjection, 2);
        const glm::vec4 W = Row(t_ViewProjection, 3);

        Frustum Out;
        Out.Planes[Left] = Normalized(W + X);
        Out.Planes[Right] = Normalized(W - X);
        Out.Planes[Bottom] = Normalized(W + Y);
        Out.Planes[Top] = Normalized(W - Y);
        Out.Planes[Near] = Normalized(Z);
        Out.Planes[Far] = Normalized(W - Z);
        return Out;
    }

    bool Frustum::IntersectsWorldBox(const glm::vec3& t_Centre, const glm::vec3& t_Extents) const
    {
        for (const glm::vec4& Plane : Planes)
        {
            const glm::vec3 Normal(Plane);
            // How far the box reaches toward the inside of the plane, from its centre
            const float Reach = glm::dot(glm::abs(Normal), t_Extents);
            // Strictly outside only: a box that touches the plane stays, and a NaN never culls
            if (glm::dot(Normal, t_Centre) + Plane.w + Reach < 0.0f)
            {
                return false;
            }
        }
        return true;
    }

    bool Frustum::Intersects(const Aabb& t_Local, const glm::mat4& t_World) const
    {
        if (!t_Local.IsValid())
        {
            return true;
        }
        // The world box around the placed local box: its centre moves with the matrix, and each
        // world half size is the sum of the absolute linear parts (exact for an affine matrix)
        const glm::vec3 LocalCentre = (t_Local.Min + t_Local.Max) * 0.5f;
        const glm::vec3 LocalExtents = (t_Local.Max - t_Local.Min) * 0.5f;
        const glm::vec3 Centre = glm::vec3(t_World * glm::vec4(LocalCentre, 1.0f));
        glm::vec3 Extents(0.0f);
        for (int Axis = 0; Axis < 3; ++Axis)
        {
            Extents += glm::abs(glm::vec3(t_World[Axis])) * LocalExtents[Axis];
        }
        return IntersectsWorldBox(Centre, Extents);
    }
} // namespace Fling
