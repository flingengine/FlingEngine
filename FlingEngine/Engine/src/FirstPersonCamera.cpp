
#include "FirstPersonCamera.h"
#include "FlingMath.h"

namespace
{
    /** Stick values with a length under this are treated as 0. */
    constexpr float StickDeadZone = 0.15f;

    /**
     * Set a stick value inside the dead zone to 0, and scale the rest so that the output
     * starts at 0 at the edge of the dead zone and is 1 at full tilt.
     */
    glm::vec2 ApplyRadialDeadZone(const glm::vec2& t_Stick)
    {
        const float Length = glm::length(t_Stick);
        if (Length <= StickDeadZone)
        {
            return glm::vec2(0.0f);
        }
        const float Scaled = glm::min((Length - StickDeadZone) / (1.0f - StickDeadZone), 1.0f);
        return t_Stick * (Scaled / Length);
    }
} // namespace

namespace Fling
{
    FirstPersonCamera::FirstPersonCamera(float aspectRatio, float t_MoveSpeed, float t_RotSpeed)
        : m_front(glm::vec3(-1.0f)), m_RotationSpeed(t_RotSpeed)
    {
        m_nearPlane = 0.1f;
        m_farPlane = 1000.0f;
        m_speed = t_MoveSpeed;
        m_aspectRatio = aspectRatio;
        m_fieldOfView = glm::radians(45.0f);
        m_position = glm::vec3(0.0f, 0.0f, 5.f);
        m_rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        m_worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

        UpdateProjectionMatrix();
    }

    FirstPersonCamera::FirstPersonCamera(glm::vec3 position, glm::vec3 rotation, float speed, float aspectRatio)
        : m_front(glm::vec3(-1.0f))
    {
        m_position = position;
        m_rotation = rotation;
        m_speed = speed;
        m_aspectRatio = aspectRatio;

        m_nearPlane = 0.1f;
        m_farPlane = 1000.0f;
        m_worldUp = glm::vec3(0.0f, 0.0f, 1.0f);
        m_fieldOfView = glm::radians(45.0f);

        UpdateProjectionMatrix();
    }

    FirstPersonCamera::FirstPersonCamera(glm::vec3 front,
                                         glm::vec3 up,
                                         glm::vec3 position,
                                         glm::vec3 rotation,
                                         float speed,
                                         float nearPlane,
                                         float farPlane,
                                         float fov,
                                         float aspectRatio)
        : m_front(front)
    {
        m_worldUp = up;
        m_position = position;
        m_rotation = rotation;
        m_speed = speed;
        m_nearPlane = nearPlane;
        m_farPlane = farPlane;
        m_fieldOfView = glm::radians(fov);
        m_aspectRatio = aspectRatio;

        UpdateProjectionMatrix();
    }

    void FirstPersonCamera::Update(float dt)
    {
        bool SlowModifier = Input::Get().IsHeld(Keys::LeftControl);

        float moveSpeed = m_speed * dt * (SlowModifier ? 0.4f : 1.0f);

        // #TODO Handle input _correctly_ with callbacks from the input manager
        // translation
        // forward
        if (Input::Get().IsHeld(Keys::W))
        {
            m_position += m_front * moveSpeed;
        }

        // backward
        if (Input::Get().IsHeld(Keys::S))
        {
            m_position -= m_front * moveSpeed;
        }

        // left
        if (Input::Get().IsHeld(Keys::A))
        {
            m_position -= m_right * moveSpeed;
        }

        // right
        if (Input::Get().IsHeld(Keys::D))
        {
            m_position += m_right * moveSpeed;
        }

        // Move world up with Q
        if (Input::Get().IsHeld(Keys::Q))
        {
            m_position += m_worldUp * moveSpeed;
        }

        // Move world down with E
        if (Input::Get().IsHeld(Keys::E))
        {
            m_position += -m_worldUp * moveSpeed;
        }

        // Gamepad: left stick moves, LB moves up, RB moves down. GLFW stick Y is -1 at the top
        const glm::vec2 MoveStick = ApplyRadialDeadZone(
            glm::vec2(Input::Get().GetAxisValue(Keys::GamepadLeftX), Input::Get().GetAxisValue(Keys::GamepadLeftY)));
        m_position += m_front * (-MoveStick.y * moveSpeed);
        m_position += m_right * (MoveStick.x * moveSpeed);

        if (Input::Get().IsHeld(Keys::GamepadLeftShoulder))
        {
            m_position += m_worldUp * moveSpeed;
        }

        if (Input::Get().IsHeld(Keys::GamepadRightShoulder))
        {
            m_position += -m_worldUp * moveSpeed;
        }

        // Gamepad: right stick rotates, in the same directions as the mouse
        const glm::vec2 LookStick = ApplyRadialDeadZone(
            glm::vec2(Input::Get().GetAxisValue(Keys::GamepadRightX), Input::Get().GetAxisValue(Keys::GamepadRightY)));
        m_rotation.x -= LookStick.x * m_GamepadLookSpeed * dt;
        m_rotation.y -= LookStick.y * m_GamepadLookSpeed * dt;
        m_rotation.y = glm::clamp(m_rotation.y, -MAX_PITCH, MAX_PITCH);

        // Rotation
        //  Check if we should rotate
        m_IsRotating = Input::Get().IsHeld(Keys::MouseRight);

        float PointerX = 0.0f;
        float PointerY = 0.0f;
        if (Input::Get().GetPointerPosition(PointerX, PointerY))
        {
            // Normalize screen coordinates
            const glm::vec2 CurMousePos(PointerX / m_ViewportWidth, PointerY / m_ViewportHeight);

            if (m_IsRotating && m_HasPrevMousePos)
            {
                float RotSpeed = dt * m_RotationSpeed;

                float MouseDeltaX = m_PrevMousePos.x - CurMousePos.x;
                float MouseDeltaY = CurMousePos.y - m_PrevMousePos.y;

                m_rotation.x += RotSpeed * MouseDeltaX;
                m_rotation.y += RotSpeed * -MouseDeltaY;

                m_rotation.y = glm::clamp(m_rotation.y, -MAX_PITCH, MAX_PITCH);
            }

            // Keep track of the mouse position
            m_PrevMousePos = CurMousePos;
            m_HasPrevMousePos = true;
        }

        UpdateCameraVectors();
        UpdateProjectionMatrix();
        UpdateViewMatrix();
    }

    void FirstPersonCamera::UpdateViewMatrix()
    {
        m_viewMatrix = glm::lookAt(m_position, m_position + m_front, m_up);
    }

    void FirstPersonCamera::UpdateProjectionMatrix()
    {
        m_projectionMatrix = glm::perspective(m_fieldOfView, m_aspectRatio, m_nearPlane, m_farPlane);
    }

    void Fling::FirstPersonCamera::UpdateCameraVectors()
    {
        glm::mat4x4 rotation = glm::eulerAngleYX(m_rotation.x, m_rotation.y);
        m_front = rotation * glm::vec4(m_worldFront, 0.f);
        m_up = rotation * glm::vec4(m_worldUp, 0.f);
        m_right = glm::cross(m_front, m_up);
    }

} // namespace Fling