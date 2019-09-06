#pragma once
#include "Camera.h"
#include "FlingMath.h"

namespace Fling 
{
    class FirstPersonCamera : public Camera
    {
        public:
            FirstPersonCamera(float width, float height);
            
            virtual void Update(float dt) override;

			
        private:
            float m_sensitivity;
            
            void UpdateViewMatrix();
            void UpdateProjectionMatrix();
    };
}