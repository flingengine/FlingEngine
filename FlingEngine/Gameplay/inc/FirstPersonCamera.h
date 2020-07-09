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

			void SetAspectRatio(float AspectRatio) { m_aspectRatio = AspectRatio; }

			bool IsRotating() const { return m_IsRotating; }

        private:
			glm::vec3 m_front;
			glm::vec3 m_up;
			glm::vec3 m_right;
			glm::vec3 m_worldUp;
			glm::vec3 m_worldFront = { 0.f, 0.f, -1.0 };
			bool m_IsRotating = false;
			MousePos m_PrevMousePos;

		PRIVATE_WITH_EDITOR:
			float m_RotationSpeed = 20.0f;

			const float MAX_PITCH = (glm::pi<float>() / 2.0f) - .017f; //subtract one degrees for gimbal lock

            void UpdateViewMatrix();
            void UpdateProjectionMatrix();
            void UpdateCameraVectors();
    };
}