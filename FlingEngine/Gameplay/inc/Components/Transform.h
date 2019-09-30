#pragma once

#include <cereal/archives/json.hpp>
#include "FlingMath.h"

namespace Fling
{
    struct Transform
    {   
        glm::mat4 GetWorldMatrix() const;

        bool operator==(const Transform &other) const;
	    bool operator!=(const Transform &other) const;
        friend std::ostream& operator << (std::ostream& t_OutStream, const Fling::Transform& t_Transform); 

        template<class Archive>
        void serialize(Archive & t_Archive);

        inline const glm::vec3& GetPos() const { return m_Pos; } 
        inline const glm::vec3& GetScale() const { return m_Scale; } 
        inline const glm::vec3& GetRotation() const { return m_Rotation; }

        void SetPos(const glm::vec3& t_Pos);
        void SetScale(const glm::vec3& t_Scale);
        void SetRotation(const glm::vec3& t_Rot);

    private:
        glm::vec3 m_Pos {};
        glm::vec3 m_Rotation {};
        glm::vec3 m_Scale {};
    };
    
    /** Serilazation to an archive */
    template<class Archive>
    void Transform::serialize(Archive & t_Archive)
    {
        t_Archive( 
            cereal::make_nvp("POS_X", m_Pos.x),
            cereal::make_nvp("POS_Y", m_Pos.y),
            cereal::make_nvp("POS_Z", m_Pos.z),

            cereal::make_nvp("SCALE_X", m_Scale.x),
            cereal::make_nvp("SCALE_Y", m_Scale.y),
            cereal::make_nvp("SCALE_Z", m_Scale.z),

            cereal::make_nvp("ROT_X", m_Rotation.x),
            cereal::make_nvp("ROT_Y", m_Rotation.y),
            cereal::make_nvp("ROT_Z", m_Rotation.z)
        );
    }
}   // namespace Fling