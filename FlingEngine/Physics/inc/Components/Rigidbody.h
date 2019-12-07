#pragma once 

#include "Serilization.h"
#include "btBulletCollisionCommon.h"

namespace Fling
{
    namespace Components
    {
        struct Rigidbody
        {
            bool operator==(const Rigidbody& other) const;
            bool operator!=(const Rigidbody& other) const;
            friend std::ostream& operator << (std::ostream& t_OutStream, 
                const Fling::Components::Rigidbody& t_Transform);

            template<class Archive>
            void serialize(Archive& t_Archive);

            void SetLinearFactor(const glm::vec3& t_LinearFactor);
            void SetAngularFactor(const glm::vec3& t_AngularFactor);
            void SetLinearVelocity(const glm::vec3& t_LinearVelocity);
            void SetAngularVelocity(const glm::vec3& t_AngularVelocity);
            void SetMass(float t_Mass);
            void SetFritcion(float t_Friction);
            void SetFrictionRolling(float t_FrictionRolling);
            void SetFritcionSpinning(float t_FrictionSpinning);

            inline const glm::vec3& GetLinearFactor() const { return m_LinearFactor; }
            inline const glm::vec3& GetAngularFactor() const { return m_AngularFactor; }
            inline const glm::vec3& GetLinearVelocity() const { return m_LinearVelocity; }
            inline const glm::vec3& GetAngularVelocity() const { return m_AngularVelocity; }
            inline float GetMass() const { return m_Mass; }
            inline float GetFriction() const { return m_Friction; }
            inline float GetFrictionRolling() const { return m_FrictionRolling; }
            inline float GetFrictionSpinning() const { return m_FrictionSpinning; }


            btTransform m_WorldTransform;

            glm::vec3 m_LinearFactor    = glm::vec3(1.0f);
            glm::vec3 m_AngularFactor   = glm::vec3(1.0f);
            glm::vec3 m_LinearVelocity  = glm::vec3(0.0f);
            glm::vec3 m_AngularVelocity = glm::vec3(0.0f);
            
            std::unique_ptr<btCollisionShape> m_Collider;
            std::unique_ptr<btRigidBody> m_Rigidbody;

            float m_Mass = 1.0f;
            float m_Friction = 0.2f;
            float m_FrictionRolling = 1.0f;
            float m_FrictionSpinning = 1.0f;
        };

        /** Serilazation to an archive */
        template<class Archive>
        void Rigidbody::serialize(Archive& t_Archive)
        {
            t_Archive(
               /* cereal::make_nvp("POS_X", m_Pos.x),
                cereal::make_nvp("POS_Y", m_Pos.y),
                cereal::make_nvp("POS_Z", m_Pos.z),

                cereal::make_nvp("SCALE_X", m_Scale.x),
                cereal::make_nvp("SCALE_Y", m_Scale.y),
                cereal::make_nvp("SCALE_Z", m_Scale.z),

                cereal::make_nvp("ROT_X", m_Rotation.x),
                cereal::make_nvp("ROT_Y", m_Rotation.y),
                cereal::make_nvp("ROT_Z", m_Rotation.z)*/
            );
        }
    }
}