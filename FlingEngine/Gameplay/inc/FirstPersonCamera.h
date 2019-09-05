#pragma once
#include "Camera.h" 

namespace fling 
{
    class FirstPersonCamera : public Camera
    {
        public:
            FirstPersonCamera(float& width, float& height);
            


        private:
            void UpdateViewMatrix();
            void UpdateProjectionMatrix();
    };
}