#pragma once 

#include "Serilization.h"
#include "btBulletCollisionCommon.h"

namespace Fling
{
    namespace Components
    {
        struct Rigidbody
        {
            Rigidbody(std::unique_ptr<btCollisionShape>& t_Collider);

            bool operator==(const Rigidbody& other) const;
            bool operator!=(const Rigidbody& other) const;
            friend std::ostream& operator << (std::ostream& t_OutStream, 
                const Fling::Components::Rigidbody& t_Transform);

            template<class Archive>
            void serialize(Archive& t_Archive);

            void SetLinearFactor(const btVector3& t_LinearFactor);
            void SetAngularFactor(const btVector3& t_AngularFactor);
            void SetLinearVelocity(const btVector3& t_LinearVelocity);
            void SetAngularVelocity(const btVector3& t_AngularVelocity);
            void SetMass(float t_Mass);
            void SetFritcion(float t_Friction);
            void SetFrictionRolling(float t_FrictionRolling);
            void SetFritcionSpinning(float t_FrictionSpinning);
            void SetCollisionShape(std::unique_ptr<btCollisionShape>& t_Collider);
            void SetGravity(const btVector3& t_Gravity);
            void RecalculateMass();

            inline const btVector3& GetLinearFactor() const { return m_LinearFactor; }
            inline const btVector3& GetAngularFactor() const { return m_AngularFactor; }
            inline const btVector3& GetLinearVelocity() const { return m_LinearVelocity; }
            inline const btVector3& GetAngularVelocity() const { return m_AngularVelocity; }
            inline float GetMass() const { return m_Mass; }
            inline float GetFriction() const { return m_Friction; }
            inline float GetFrictionRolling() const { return m_FrictionRolling; }
            inline float GetFrictionSpinning() const { return m_FrictionSpinning; }
            
            btVector3& GetLocalInertia() { return m_LocalInertia; }

            btTransform m_WorldTransform;
            btVector3 m_LocalInertia;

            btVector3 m_LinearFactor    = btVector3(1.0f, 1.0f, 1.0f);
            btVector3 m_AngularFactor   = btVector3(1.0f, 1.0f, 1.0f);
            btVector3 m_LinearVelocity  = btVector3(0.0f, 0.0f, 0.0f);
            btVector3 m_AngularVelocity = btVector3(0.0f, 0.0f, 0.0f);
            
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