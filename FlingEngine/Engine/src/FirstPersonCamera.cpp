
#include "FirstPersonCamera.h"
#include "FlingMath.h"
#include "Input/Input.h"

#include <cmath>

namespace
{
    using namespace Fling;

    /** True if the key is held on any device, and UI does not own it. */
    bool IsHeldOnAny(const InputDeviceStates& t_State, const InputCapture t_Capture, const InputKey t_Key)
    {
        if (IsKeyCaptured(t_Capture, t_Key))
        {
            return false;
        }
        for (const auto& Entry : t_State.GetDevices())
        {
            if (Entry.second.GetKeyState(t_Key).IsHeld)
            {
                return true;
            }
        }
        return false;
    }

    /** The largest absolute axis value of the key over every device, with its sign. */
    float LargestAxisValue(const InputDeviceStates& t_State, const InputKey t_Key)
    {
        float Best = 0.0f;
        for (const auto& Entry : t_State.GetDevices())
        {
            const float Value = Entry.second.GetKeyState(t_Key).Value;
            if (std::fabs(Value) > std::fabs(Best))
            {
                Best = Value;
            }
        }
        return Best;
    }

    /** The cursor position of the first connected device with the Pointer capability. False if there is none. */
    bool PointerPosition(const InputDeviceStates& t_State, float& t_OutX, float& t_OutY)
    {
        for (const std::shared_ptr<InputDevice>& Device : Input::Get().GetDevices())
        {
            if (!Device->GetInfo().HasCapability(InputDeviceCapabilities::Pointer))
            {
                continue;
            }
            const InputDeviceState* State = t_State.Find(Device->GetId());
            if (State == nullptr)
            {
                return false;
            }
            t_OutX = State->PointerX;
            t_OutY = State->PointerY;
            return true;
        }
        return false;
    }

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

    void FirstPersonCamera::ReadInput()
    {
        Input& Engine = Input::Get();
        if (!Engine.IsInitialized())
        {
            m_Input.reset();
            return;
        }
        if (!m_Input || !m_Input->IsValid())
        {
            m_Input.emplace(Engine.CreateProcessor());
        }
        if (m_Input->Overran())
        {
            const InputStateSnapshot Latest = Engine.GetHandler()->GetLatestSnapshot();
            m_Input->Resync(Latest.States, Latest.Sequence);
        }
        m_Input->ConsumeUntil();
        m_Input->ClearEdges();
    }

    void FirstPersonCamera::Update(float dt)
    {
        ReadInput();
        const InputDeviceStates NoInput;
        const InputDeviceStates& State = m_Input ? m_Input->GetState() : NoInput;
        const InputCapture Capture = MakeCapture(Input::Get().IsKeyboardCaptured(), Input::Get().IsMouseCaptured());
        const auto Held = [&](const InputKey t_Key)
        {
            return IsHeldOnAny(State, Capture, t_Key);
        };
        const auto Axis = [&](const InputKey t_Key)
        {
            return LargestAxisValue(State, t_Key);
        };

        bool SlowModifier = Held(Keys::LeftControl);

        float moveSpeed = m_speed * dt * (SlowModifier ? 0.4f : 1.0f);

        // #TODO Handle input _correctly_ with callbacks from the input manager
        // translation
        // forward
        if (Held(Keys::W))
        {
            m_position += m_front * moveSpeed;
        }

        // backward
        if (Held(Keys::S))
        {
            m_position -= m_front * moveSpeed;
        }

        // left
        if (Held(Keys::A))
        {
            m_position -= m_right * moveSpeed;
        }

        // right
        if (Held(Keys::D))
        {
            m_position += m_right * moveSpeed;
        }

        // Move world up with Q
        if (Held(Keys::Q))
        {
            m_position += m_worldUp * moveSpeed;
        }

        // Move world down with E
        if (Held(Keys::E))
        {
            m_position += -m_worldUp * moveSpeed;
        }

        // Gamepad: left stick moves, LB moves up, RB moves down. GLFW stick Y is -1 at the top
        const glm::vec2 MoveStick = ApplyRadialDeadZone(glm::vec2(Axis(Keys::GamepadLeftX), Axis(Keys::GamepadLeftY)));
        m_position += m_front * (-MoveStick.y * moveSpeed);
        m_position += m_right * (MoveStick.x * moveSpeed);

        if (Held(Keys::GamepadLeftShoulder))
        {
            m_position += m_worldUp * moveSpeed;
        }

        if (Held(Keys::GamepadRightShoulder))
        {
            m_position += -m_worldUp * moveSpeed;
        }

        // Gamepad: right stick rotates, in the same directions as the mouse
        const glm::vec2 LookStick =
            ApplyRadialDeadZone(glm::vec2(Axis(Keys::GamepadRightX), Axis(Keys::GamepadRightY)));
        m_rotation.x -= LookStick.x * m_GamepadLookSpeed * dt;
        m_rotation.y -= LookStick.y * m_GamepadLookSpeed * dt;
        m_rotation.y = glm::clamp(m_rotation.y, -MAX_PITCH, MAX_PITCH);

        // Rotation
        //  Check if we should rotate
        m_IsRotating = Held(Keys::MouseRight);

        float PointerX = 0.0f;
        float PointerY = 0.0f;
        if (PointerPosition(State, PointerX, PointerY))
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

    void FirstPersonCamera::SetLookRotation(const float t_Yaw, const float t_Pitch)
    {
        m_rotation.x = t_Yaw;
        m_rotation.y = glm::clamp(t_Pitch, -MAX_PITCH, MAX_PITCH);
        UpdateCameraVectors();
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