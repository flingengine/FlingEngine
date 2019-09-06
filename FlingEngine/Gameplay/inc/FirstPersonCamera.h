#pragma once
#include "Camera.h"

namespace Fling 
{
    class FirstPersonCamera : public Camera
    {
        public:
            FirstPersonCamera(float aspectRatio);
            
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