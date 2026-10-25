#pragma once

#include "FlingTypes.h"
#include <cstddef>
#include <functional>
#include <string_view>

namespace Fling
{
    /** What kind of input a key is. A key can have more than one flag. */
    enum class InputKeyFlags : uint8
    {
        None = 0,
        Keyboard = 1 << 0,
        Mouse = 1 << 1,
        Gamepad = 1 << 2,
        /** An analog value (stick, trigger) instead of a button. */
        Axis = 1 << 3,
        /** Shift, Ctrl, Alt, or Super. */
        Modifier = 1 << 4,
    };

    constexpr InputKeyFlags operator|(InputKeyFlags t_A, InputKeyFlags t_B)
    {
        return static_cast<InputKeyFlags>(static_cast<uint8>(t_A) | static_cast<uint8>(t_B));
    }

    constexpr InputKeyFlags operator&(InputKeyFlags t_A, InputKeyFlags t_B)
    {
        return static_cast<InputKeyFlags>(static_cast<uint8>(t_A) & static_cast<uint8>(t_B));
    }

    /** The ID of every built-in key. The order is not stable between builds, so never save it. Save the name. */
    enum class InputKeyId : uint16
    {
        Invalid = 0,
#define FLING_INPUT_KEY(Id, DisplayName, Flags) Id,
#include "Input/InputKeyList.inl"
#undef FLING_INPUT_KEY
        Count
    };

    /** Static data about one key. */
    struct InputKeyDetails
    {
        /** The name that code and files use, for example "LeftShift". */
        const char* Name;

        /** The name to show to a player, for example "Left Shift". */
        const char* DisplayName;

        InputKeyFlags Flags;
    };

    /**
     * A platform-agnostic name for one button or axis on a device.
     * Backends map their own key codes to an InputKey. Files store a key by its name
     * (see GetName and FromName), so recordings work on every platform.
     */
    class InputKey
    {
    public:
        constexpr InputKey() = default;

        constexpr explicit InputKey(InputKeyId t_Id) : m_Id(t_Id) {}

        constexpr InputKeyId GetId() const { return m_Id; }

        constexpr bool IsValid() const { return m_Id != InputKeyId::Invalid && m_Id < InputKeyId::Count; }

        /** The details of this key. An invalid key gives the details of "Invalid". */
        const InputKeyDetails& GetDetails() const;

        const char* GetName() const { return GetDetails().Name; }

        const char* GetDisplayName() const { return GetDetails().DisplayName; }

        bool HasFlag(InputKeyFlags t_Flag) const { return (GetDetails().Flags & t_Flag) != InputKeyFlags::None; }

        bool IsAxis() const { return HasFlag(InputKeyFlags::Axis); }

        bool IsKeyboardKey() const { return HasFlag(InputKeyFlags::Keyboard); }

        bool IsMouseButton() const { return HasFlag(InputKeyFlags::Mouse); }

        bool IsGamepadKey() const { return HasFlag(InputKeyFlags::Gamepad); }

        bool IsModifier() const { return HasFlag(InputKeyFlags::Modifier); }

        /** Find a key by its name (case sensitive). Returns an invalid key if no key has this name. */
        static InputKey FromName(std::string_view t_Name);

        constexpr bool operator==(const InputKey& t_Other) const { return m_Id == t_Other.m_Id; }

        constexpr bool operator!=(const InputKey& t_Other) const { return m_Id != t_Other.m_Id; }

        constexpr bool operator<(const InputKey& t_Other) const { return m_Id < t_Other.m_Id; }

    private:
        InputKeyId m_Id = InputKeyId::Invalid;
    };

    /** Every built-in key, for example Keys::W or Keys::GamepadLeftX. */
    namespace Keys
    {
        inline constexpr InputKey Invalid{InputKeyId::Invalid};
#define FLING_INPUT_KEY(Id, DisplayName, Flags) inline constexpr InputKey Id{InputKeyId::Id};
#include "Input/InputKeyList.inl"
#undef FLING_INPUT_KEY
    } // namespace Keys
} // namespace Fling

template <>
struct std::hash<Fling::InputKey>
{
    std::size_t operator()(const Fling::InputKey& t_Key) const noexcept
    {
        return static_cast<std::size_t>(t_Key.GetId());
    }
};
