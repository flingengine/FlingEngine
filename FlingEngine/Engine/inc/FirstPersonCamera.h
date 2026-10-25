#pragma once
#include "Camera.h"
#include "Input/Input.h"

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

			/** Set the size of the view in pixels. Mouse movement is divided by this size. */
			void SetViewportSize(float t_Width, float t_Height) { m_ViewportWidth = t_Width; m_ViewportHeight = t_Height; }

			bool IsRotating() const { return m_IsRotating; }

        private:
			glm::vec3 m_front;
			glm::vec3 m_up;
			glm::vec3 m_right;
			glm::vec3 m_worldUp;
			glm::vec3 m_worldFront = { 0.f, 0.f, -1.0 };
			bool m_IsRotating = false;
			/** The last normalized cursor position. */
			glm::vec2 m_PrevMousePos = glm::vec2(0.0f);
			/** True if m_PrevMousePos was read from a pointer device. */
			bool m_HasPrevMousePos = false;
			float m_ViewportWidth = 1.0f;
			float m_ViewportHeight = 1.0f;

		PRIVATE_WITH_EDITOR:
			float m_RotationSpeed = 20.0f;

			/** Gamepad look speed at full stick tilt, in radians per second. */
			float m_GamepadLookSpeed = 2.5f;

			const float MAX_PITCH = (glm::pi<float>() / 2.0f) - .017f; //subtract one degrees for gimbal lock

            void UpdateViewMatrix();
            void UpdateProjectionMatrix();
            void UpdateCameraVectors();
    };
}