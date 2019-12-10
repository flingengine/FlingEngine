#pragma once 
#include <memory>

#include "btBulletCollisionCommon.h"
#include "FlingMath.h"
#include "Serilization.h"

namespace Fling
{
    namespace Components
    {
        struct BoxCollider
        {
            bool operator==(const BoxCollider& other) const;
            bool operator!=(const BoxCollider& other) const;
            friend std::ostream& operator << (
                std::ostream& t_Outstream,
                Fling::Components::BoxCollider& t_BoxCollider);

            template<class Archive>
            void serialize(Archive& t_Archive);

            void SetBoxCollider(const glm::vec3& t_BoxCollider);

            glm::vec3 m_BoxCollider = { 1.0f, 1.0f, 1.0f };
            std::unique_ptr<btCollisionShape> m_BtBoxCollider;
        };

        //template<class Archive>
        //void BoxCollider::serialize(Archive& t_Archive)
        //{
        //    t_Archive(
        //        cereal::make_nvp("BOXCOLLIDER_X", m_BoxCollider.x),
        //        cereal::make_nvp("BOXCOLLIDER_Y", m_BoxCollider.y),
        //        cereal::make_nvp("BOXCOLLIDER_Z", m_BoxCollider.z)
        //    );
        //}
    }
}