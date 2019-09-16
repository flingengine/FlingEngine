#pragma once
#include "Camera.h"

namespace Fling 
{
	/**
	 * A simple first person camera. Moves with WASD and rotates with right click + drag
	 */
    class FirstPersonCamera : public Camera
    {
        public:
            FirstPersonCamera(float aspectRatio, float t_MoveSpeed = 10.0f, float t_RotSpeed = 40.f);

			FirstPersonCamera(
				glm::vec3 position, 
				glm::vec3 rotation, 
				float speed, 
				float aspectRatio);

			FirstPersonCamera(
				glm::vec3 front, glm::vec3 up,
				glm::vec3 position, glm::vec3 rotation,
				float speed,
				float nearPlane, float farPlane,
				float fov, float aspectRatio);
			
			void Update(float dt) override;

			float GetRotationSpeed() const { return m_RotationSpeed; }
			void SetRotationSpeed(float t_NewSpeed) { m_RotationSpeed = t_NewSpeed; }

			bool IsRotating() const { return m_IsRotating; }

        private:
			glm::vec3 m_front;
			glm::vec3 m_up;
			glm::vec3 m_right;
			glm::vec3 m_worldUp;

			float m_RotationSpeed = 20.0f;
			bool m_IsRotating = false;

			MousePos m_PrevMousePos;

            void UpdateViewMatrix();
            void UpdateProjectionMatrix();
            void UpdateCameraVectors();
    };
}