#include "TransformUtil.h"

namespace Fling::TransformUtil
{
    glm::mat4 LocalMatrix(const Transform& t_Transform)
    {
        glm::mat4 Result = glm::translate(glm::mat4(1.0f), t_Transform.Position);
        Result = Result * glm::mat4_cast(t_Transform.Rotation);
        return glm::scale(Result, t_Transform.Scale);
    }

    glm::quat FromEulerDegrees(const glm::vec3& t_Degrees)
    {
        return glm::quat_cast(
            glm::yawPitchRoll(glm::radians(t_Degrees.y), glm::radians(t_Degrees.x), glm::radians(t_Degrees.z)));
    }

    glm::vec3 ToEulerDegrees(const glm::quat& t_Rotation)
    {
        // The inverse of yawPitchRoll(yaw, pitch, roll) = Ry(yaw) * Rx(pitch) * Rz(roll)
        const glm::mat3 M = glm::mat3_cast(glm::normalize(t_Rotation));
        float Yaw = 0.0f;
        float Pitch = 0.0f;
        float Roll = 0.0f;
        glm::extractEulerAngleYXZ(glm::mat4(M), Yaw, Pitch, Roll);
        return glm::degrees(glm::vec3(Pitch, Yaw, Roll));
    }

    void RotateAround(Transform& t_Transform, const glm::vec3& t_Axis, const float t_Degrees)
    {
        t_Transform.Rotation =
            glm::normalize(glm::angleAxis(glm::radians(t_Degrees), glm::normalize(t_Axis)) * t_Transform.Rotation);
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
