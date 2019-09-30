#include "pch.h"

#include "Components/Transform.h"

namespace Fling
{
    bool Transform::operator==(const Transform &other) const 
    {
        return m_Pos == other.m_Pos && m_Rotation == other.m_Rotation && m_Scale == other.m_Scale;
    }

    bool Transform::operator!=(const Transform &other) const 
    {
        return !(*this == other);
    }
    
    std::ostream& operator<< (std::ostream& t_OutStream, const Fling::Transform& t_Transform)   
    {
        t_OutStream << "Pos (" << t_Transform.m_Pos.x << "," << t_Transform.m_Pos.y << "," << t_Transform.m_Pos.z << ")";
        t_OutStream << ", Scale (" << t_Transform.m_Scale.x << "," << t_Transform.m_Scale.y << "," << t_Transform.m_Scale.z << ")";
        t_OutStream << ", Rot (" << t_Transform.m_Rotation.x << "," << t_Transform.m_Rotation.y << "," << t_Transform.m_Rotation.z << ")";

        return t_OutStream;
    }

    glm::mat4 Transform::GetWorldMatrix() const
    {
        // World = Scale * rot * pos
        glm::mat4 worldMat = glm::identity<glm::mat4>();
        worldMat = glm::translate( worldMat, m_Pos );
        worldMat = worldMat * glm::yawPitchRoll(glm::radians(m_Rotation.y), glm::radians(m_Rotation.x), glm::radians(m_Rotation.z) );
        worldMat = glm::scale( worldMat, m_Scale );

        return worldMat;
        //return glm::transpose( worldMat );
    }

    void Transform::SetPos(const glm::vec3& t_Pos)
    {
        m_Pos = t_Pos;
    }

    void Transform::SetScale(const glm::vec3& t_Scale)
    {
        m_Scale = t_Scale;
    }

    void Transform::SetRotation(const glm::vec3& t_Rot)
    {
        m_Rotation = t_Rot;
    }
}   // namespace Fling