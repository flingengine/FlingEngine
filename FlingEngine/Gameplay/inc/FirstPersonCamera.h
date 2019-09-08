#pragma once
#include "Camera.h"

namespace Fling 
{
    class FirstPersonCamera : public Camera
    {
        public:
            FirstPersonCamera(float aspectRatio);

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

        private:
			glm::vec3 m_front;
			glm::vec3 m_up;
			glm::vec3 m_right;
			glm::vec3 m_worldUp;

            void UpdateViewMatrix();
            void UpdateProjectionMatrix();
			void UpdateCameraVectors();
    };
}