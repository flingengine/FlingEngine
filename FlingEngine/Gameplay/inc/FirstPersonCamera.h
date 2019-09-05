#pragma once
#include "Camera.h" 

namespace fling 
{
    class FirstPersonCamera : public Camera
    {
        public:
            FirstPersonCamera(float& width, float& height);
            
            void Update() override;


        private:
            float m_sensitivity;
            
            void UpdateViewMatrix();
            void UpdateProjectionMatrix();
    };
}