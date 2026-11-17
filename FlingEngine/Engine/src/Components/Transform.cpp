#include "SimMath.h"
#include "TransformUtil.h"

namespace Fling::TransformUtil
{
    glm::mat4 LocalMatrix(const Transform& t_Transform)
    {
        // Translate * rotate * scale, written out: the same values as the three glm calls
        // (no matrix products), and the draw loop makes one of these for each moving mesh each frame
        const glm::mat3 Rotation = glm::mat3_cast(t_Transform.Rotation);
        glm::mat4 Result;
        Result[0] = glm::vec4(Rotation[0] * t_Transform.Scale.x, 0.0f);
        Result[1] = glm::vec4(Rotation[1] * t_Transform.Scale.y, 0.0f);
        Result[2] = glm::vec4(Rotation[2] * t_Transform.Scale.z, 0.0f);
        Result[3] = glm::vec4(t_Transform.Position, 1.0f);
        return Result;
    }

    glm::quat FromEulerDegrees(const glm::vec3& t_Degrees)
    {
        // SimMath, not glm::yawPitchRoll: a level loaded on Linux and on Windows must start from the same bits
        return SimMath::FromYawPitchRoll(
            glm::radians(t_Degrees.y), glm::radians(t_Degrees.x), glm::radians(t_Degrees.z));
    }

    glm::vec3 ToEulerDegrees(const glm::quat& t_Rotation)
    {
        // The inverse of FromEulerDegrees: Ry(yaw) * Rx(pitch) * Rz(roll)
        const glm::mat3 M = glm::mat3_cast(glm::normalize(t_Rotation));
        float Yaw = 0.0f;
        float Pitch = 0.0f;
        float Roll = 0.0f;
        SimMath::ToYawPitchRoll(M, Yaw, Pitch, Roll);
        return glm::degrees(glm::vec3(Pitch, Yaw, Roll));
    }

    void RotateAround(Transform& t_Transform, const glm::vec3& t_Axis, const float t_Degrees)
    {
        // SimMath, not glm::angleAxis, so that every platform turns by the same bits
        const glm::quat Turn = SimMath::AngleAxis(glm::radians(t_Degrees), glm::normalize(t_Axis));
        t_Transform.Rotation = glm::normalize(Turn * t_Transform.Rotation);
    }

    glm::vec3 Forward(const Transform& t_Transform)
    {
        return t_Transform.Rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    }

    glm::vec3 Right(const Transform& t_Transform)
    {
        return t_Transform.Rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::vec3 Up(const Transform& t_Transform)
    {
        return t_Transform.Rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    }
} // namespace Fling::TransformUtil
