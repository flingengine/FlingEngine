#include "Input/InputRecording.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>

namespace Fling
{
    namespace
    {
        using Json = nlohmann::json;

        struct CapabilityName
        {
            InputDeviceCapabilities Capability;
            const char* Text;
        };

        constexpr CapabilityName CapabilityNames[] = {
            {InputDeviceCapabilities::Keys, "Keys"},
            {InputDeviceCapabilities::MouseButtons, "MouseButtons"},
            {InputDeviceCapabilities::Pointer, "Pointer"},
            {InputDeviceCapabilities::RelativeMotion, "RelativeMotion"},
            {InputDeviceCapabilities::Wheel, "Wheel"},
            {InputDeviceCapabilities::Buttons, "Buttons"},
            {InputDeviceCapabilities::Thumbsticks, "Thumbsticks"},
            {InputDeviceCapabilities::Triggers, "Triggers"},
            {InputDeviceCapabilities::Haptics, "Haptics"},
            {InputDeviceCapabilities::Lighting, "Lighting"},
            {InputDeviceCapabilities::Gyro, "Gyro"},
            {InputDeviceCapabilities::Accelerometer, "Accelerometer"},
            {InputDeviceCapabilities::Touchpad, "Touchpad"},
        };

        constexpr InputDeviceType DeviceTypes[] = {InputDeviceType::Keyboard,
                                                   InputDeviceType::Mouse,
                                                   InputDeviceType::Gamepad,
                                                   InputDeviceType::Touch,
                                                   InputDeviceType::Virtual};

        /** The message types that a file can contain, besides "Press". */
        constexpr InputMessageType FileMessageTypes[] = {InputMessageType::KeyDown,
                                                         InputMessageType::KeyUp,
                                                         InputMessageType::KeyRepeat,
                                                         InputMessageType::Char,
                                                         InputMessageType::Axis,
                                                         InputMessageType::MouseMove,
                                                         InputMessageType::MouseWheel,
                                                         InputMessageType::FocusLost,
                                                         InputMessageType::FocusGained};

        bool ParseDeviceType(std::string_view t_Name, InputDeviceType& t_OutType)
        {
            for (const InputDeviceType Type : DeviceTypes)
            {
                if (t_Name == ToString(Type))
                {
                    t_OutType = Type;
                    return true;
                }
            }
            return false;
        }

        bool ParseCapability(std::string_view t_Name, InputDeviceCapabilities& t_OutCapability)
        {
            for (const CapabilityName& Item : CapabilityNames)
            {
                if (t_Name == Item.Text)
                {
                    t_OutCapability = Item.Capability;
                    return true;
                }
            }
            return false;
        }

        InputDeviceCapabilities DefaultCapabilities(InputDeviceType t_Type)
        {
            switch (t_Type)
            {
            case InputDeviceType::Keyboard:
                return InputDeviceCapabilities::Keys;
            case InputDeviceType::Mouse:
                return InputDeviceCapabilities::MouseButtons | InputDeviceCapabilities::Pointer |
                       InputDeviceCapabilities::RelativeMotion | InputDeviceCapabilities::Wheel;
            case InputDeviceType::Gamepad:
                return InputDeviceCapabilities::Buttons | InputDeviceCapabilities::Thumbsticks |
                       InputDeviceCapabilities::Triggers;
            default:
                return InputDeviceCapabilities::None;
            }
        }

        /** Round to 1 microsecond, so a written time reads back as the same number. */
        double RoundMicroseconds(double t_Seconds)
        {
            return std::round(t_Seconds * 1e6) / 1e6;
        }

        /** Decode UTF-8 into code points. Returns false for invalid UTF-8. */
        bool DecodeUtf8(const std::string& t_Text, std::vector<uint32>& t_OutCodePoints)
        {
            size_t i = 0;
            while (i < t_Text.size())
            {
                const unsigned char Lead = static_cast<unsigned char>(t_Text[i]);
                uint32 CodePoint = 0;
                size_t Extra = 0;
                if (Lead < 0x80)
                {
                    CodePoint = Lead;
                }
                else if ((Lead & 0xE0) == 0xC0)
                {
                    CodePoint = Lead & 0x1F;
                    Extra = 1;
                }
                else if ((Lead & 0xF0) == 0xE0)
                {
                    CodePoint = Lead & 0x0F;
                    Extra = 2;
                }
                else if ((Lead & 0xF8) == 0xF0)
                {
                    CodePoint = Lead & 0x07;
                    Extra = 3;
                }
                else
                {
                    return false;
                }
                for (size_t k = 1; k <= Extra; ++k)
                {
                    if (i + k >= t_Text.size())
                    {
                        return false;
                    }
                    const unsigned char Next = static_cast<unsigned char>(t_Text[i + k]);
                    if ((Next & 0xC0) != 0x80)
                    {
                        return false;
                    }
                    CodePoint = (CodePoint << 6) | (Next & 0x3F);
                }
                t_OutCodePoints.push_back(CodePoint);
                i += Extra + 1;
            }
            return true;
        }

        /** A message with the data that is only known while parsing. */
        struct ParsedMessage
        {
            InputRecordedMessage Recorded;

            /** MouseMove only: the file gave no x / no y, so use the last position plus the delta. */
            bool NeedsX = false;
            bool NeedsY = false;
        };

        /** Reads one parse of a recording. Each Parse function returns an empty string on success. */
        class RecordingParser
        {
        public:
            explicit RecordingParser(InputRecording& t_Out) : m_Out(t_Out) {}

            std::string Run(std::string_view t_Text)
            {
                m_Out = InputRecording{};
                bool HaveHeader = false;
                size_t LineNumber = 0;
                size_t Pos = 0;
                while (Pos <= t_Text.size())
                {
                    size_t End = t_Text.find('\n', Pos);
                    if (End == std::string_view::npos)
                    {
                        End = t_Text.size();
                    }
                    std::string_view Line = t_Text.substr(Pos, End - Pos);
                    Pos = End + 1;
                    ++LineNumber;

                    while (!Line.empty() && (Line.back() == '\r' || Line.back() == ' ' || Line.back() == '\t'))
                    {
                        Line.remove_suffix(1);
                    }
                    size_t Start = 0;
                    while (Start < Line.size() && (Line[Start] == ' ' || Line[Start] == '\t'))
                    {
                        ++Start;
                    }
                    Line.remove_prefix(Start);
                    if (Line.empty() || Line.front() == '#')
                    {
                        continue;
                    }

                    const Json Value = Json::parse(Line.begin(), Line.end(), nullptr, false);
                    std::string Error;
                    if (Value.is_discarded())
                    {
                        Error = "not valid JSON";
                    }
                    else if (!Value.is_object())
                    {
                        Error = "expected a JSON object";
                    }
                    else if (!HaveHeader)
                    {
                        Error = ParseHeader(Value);
                        HaveHeader = true;
                    }
                    else
                    {
                        Error = ParseMessage(Value);
                    }
                    if (!Error.empty())
                    {
                        return "line " + std::to_string(LineNumber) + ": " + Error;
                    }
                }

                if (!HaveHeader)
                {
                    return "line 1: missing header";
                }
                Finish();
                return {};
            }

        private:
            static std::string Quote(const std::string& t_Text) { return "'" + t_Text + "'"; }

            /** Read an optional number field. Returns false (with t_Error set) if it is not a number. */
            static bool
            ReadNumber(const Json& t_Object, const char* t_Name, std::optional<double>& t_Out, std::string& t_Error)
            {
                t_Out.reset();
                const auto It = t_Object.find(t_Name);
                if (It == t_Object.end())
                {
                    return true;
                }
                if (!It->is_number())
                {
                    t_Error = std::string("\"") + t_Name + "\" must be a number";
                    return false;
                }
                t_Out = It->get<double>();
                return true;
            }

            /** Read an optional whole number field in the range of a 32-bit value. */
            static bool ReadInteger(const Json& t_Object,
                                    const char* t_Name,
                                    std::optional<int64>& t_Out,
                                    std::string& t_Error,
                                    int64 t_Min,
                                    int64 t_Max)
            {
                t_Out.reset();
                const auto It = t_Object.find(t_Name);
                if (It == t_Object.end())
                {
                    return true;
                }
                if (!It->is_number_integer())
                {
                    t_Error = std::string("\"") + t_Name + "\" must be a whole number";
                    return false;
                }
                const int64 Value = It->get<int64>();
                if (Value < t_Min || Value > t_Max)
                {
                    t_Error = std::string("\"") + t_Name + "\" is out of range";
                    return false;
                }
                t_Out = Value;
                return true;
            }

            std::string ParseHeader(const Json& t_Header)
            {
                const auto Format = t_Header.find("format");
                if (Format == t_Header.end() || !Format->is_string() ||
                    Format->get<std::string>() != InputRecording::FormatName)
                {
                    return std::string("header: \"format\" must be \"") + InputRecording::FormatName + "\"";
                }
                std::string Error;
                std::optional<int64> Version;
                if (!ReadInteger(t_Header, "version", Version, Error, 0, std::numeric_limits<uint32>::max()))
                {
                    return "header: " + Error;
                }
                if (!Version)
                {
                    return "header: missing \"version\"";
                }
                if (*Version != static_cast<int64>(InputRecording::CurrentVersion))
                {
                    return "header: unsupported version " + std::to_string(*Version);
                }
                m_Out.Version = static_cast<uint32>(*Version);

                std::optional<int64> TickRate;
                if (!ReadInteger(t_Header, "tickRate", TickRate, Error, 1, std::numeric_limits<uint32>::max()))
                {
                    return "header: " + Error;
                }
                if (TickRate)
                {
                    m_Out.TickRate = static_cast<uint32>(*TickRate);
                }

                std::optional<double> Duration;
                if (!ReadNumber(t_Header, "duration", Duration, Error))
                {
                    return "header: " + Error;
                }
                if (Duration)
                {
                    if (*Duration < 0.0)
                    {
                        return "header: \"duration\" must not be negative";
                    }
                    m_Out.Duration = *Duration;
                }

                const auto Devices = t_Header.find("devices");
                if (Devices != t_Header.end())
                {
                    if (!Devices->is_array())
                    {
                        return "header: \"devices\" must be a list";
                    }
                    for (const Json& Entry : *Devices)
                    {
                        Error = ParseDevice(Entry);
                        if (!Error.empty())
                        {
                            return "header: " + Error;
                        }
                    }
                }
                return {};
            }

            std::string ParseDevice(const Json& t_Entry)
            {
                if (!t_Entry.is_object())
                {
                    return "a device must be an object";
                }
                std::string Error;
                std::optional<int64> Id;
                if (!ReadInteger(t_Entry, "id", Id, Error, 0, std::numeric_limits<uint32>::max()))
                {
                    return "device " + Error;
                }
                if (!Id)
                {
                    return "a device has no \"id\"";
                }
                if (m_Out.FindDevice(static_cast<uint32>(*Id)) != nullptr)
                {
                    return "device id " + std::to_string(*Id) + " is used twice";
                }

                InputRecordingDevice Device;
                Device.Id = static_cast<uint32>(*Id);

                const auto Type = t_Entry.find("type");
                if (Type == t_Entry.end() || !Type->is_string())
                {
                    return "device " + std::to_string(*Id) + " has no \"type\"";
                }
                if (!ParseDeviceType(Type->get<std::string>(), Device.Info.Type))
                {
                    return "device " + std::to_string(*Id) + " has an unknown type " + Quote(Type->get<std::string>());
                }

                const auto Name = t_Entry.find("name");
                if (Name != t_Entry.end() && Name->is_string())
                {
                    Device.Info.DisplayName = Name->get<std::string>();
                }
                const auto Path = t_Entry.find("path");
                if (Path != t_Entry.end() && Path->is_string())
                {
                    Device.Info.HardwarePath = Path->get<std::string>();
                }
                std::optional<int64> Number;
                if (!ReadInteger(t_Entry, "vendorId", Number, Error, 0, 0xFFFF))
                {
                    return "device " + Error;
                }
                Device.Info.VendorId = static_cast<uint16>(Number.value_or(0));
                if (!ReadInteger(t_Entry, "productId", Number, Error, 0, 0xFFFF))
                {
                    return "device " + Error;
                }
                Device.Info.ProductId = static_cast<uint16>(Number.value_or(0));

                const auto Capabilities = t_Entry.find("capabilities");
                if (Capabilities == t_Entry.end())
                {
                    Device.Info.Capabilities = DefaultCapabilities(Device.Info.Type);
                }
                else
                {
                    if (!Capabilities->is_array())
                    {
                        return "\"capabilities\" must be a list";
                    }
                    for (const Json& Item : *Capabilities)
                    {
                        InputDeviceCapabilities Capability = InputDeviceCapabilities::None;
                        if (!Item.is_string() || !ParseCapability(Item.get<std::string>(), Capability))
                        {
                            return "unknown capability " +
                                   (Item.is_string() ? Quote(Item.get<std::string>()) : "value");
                        }
                        Device.Info.Capabilities = Device.Info.Capabilities | Capability;
                    }
                }

                m_NextDeviceId = std::max(m_NextDeviceId, Device.Id + 1);
                m_Out.Devices.push_back(std::move(Device));
                return {};
            }

            /** The first device of this type, or a new one. */
            uint32 FindOrAddDevice(InputDeviceType t_Type)
            {
                for (const InputRecordingDevice& Device : m_Out.Devices)
                {
                    if (Device.Info.Type == t_Type)
                    {
                        return Device.Id;
                    }
                }
                InputRecordingDevice Device;
                Device.Id = m_NextDeviceId++;
                Device.Info.Type = t_Type;
                Device.Info.DisplayName = ToString(t_Type);
                Device.Info.Capabilities = DefaultCapabilities(t_Type);
                m_Out.Devices.push_back(Device);
                return Device.Id;
            }

            std::string ParseMessage(const Json& t_Line)
            {
                std::string Error;

                // Time
                std::optional<double> Seconds;
                std::optional<int64> Tick;
                if (!ReadNumber(t_Line, "t", Seconds, Error) ||
                    !ReadInteger(t_Line, "tick", Tick, Error, 0, std::numeric_limits<int64>::max()))
                {
                    return Error;
                }
                if (Seconds && Tick)
                {
                    return "use \"t\" or \"tick\", not both";
                }
                if (!Seconds && !Tick)
                {
                    return "missing \"t\" or \"tick\"";
                }
                double Time = 0.0;
                if (Seconds)
                {
                    if (*Seconds < 0.0)
                    {
                        return "\"t\" must not be negative";
                    }
                    Time = *Seconds;
                }
                else
                {
                    Time = static_cast<double>(*Tick) / static_cast<double>(m_Out.TickRate);
                }

                // Type
                const auto TypeField = t_Line.find("type");
                if (TypeField == t_Line.end() || !TypeField->is_string())
                {
                    return "missing \"type\"";
                }
                const std::string TypeName = TypeField->get<std::string>();
                const bool IsPress = TypeName == "Press";
                InputMessageType Type = InputMessageType::KeyDown;
                bool Found = IsPress;
                for (const InputMessageType Candidate : FileMessageTypes)
                {
                    if (TypeName == ToString(Candidate))
                    {
                        Type = Candidate;
                        Found = true;
                        break;
                    }
                }
                if (!Found)
                {
                    return "unknown message type " + Quote(TypeName);
                }

                ParsedMessage Message;
                Message.Recorded.TimeSeconds = Time;
                Message.Recorded.Message.Type = Type;

                // Key
                const bool UsesKey = IsPress || Type == InputMessageType::KeyDown || Type == InputMessageType::KeyUp ||
                                     Type == InputMessageType::KeyRepeat || Type == InputMessageType::Axis;
                if (UsesKey)
                {
                    const auto KeyField = t_Line.find("key");
                    if (KeyField == t_Line.end() || !KeyField->is_string())
                    {
                        return "missing \"key\"";
                    }
                    const InputKey Key = InputKey::FromName(KeyField->get<std::string>());
                    if (!Key.IsValid())
                    {
                        return "unknown key " + Quote(KeyField->get<std::string>());
                    }
                    if (Type == InputMessageType::Axis && !Key.IsAxis())
                    {
                        return "key " + Quote(Key.GetName()) + " is not an axis";
                    }
                    Message.Recorded.Message.Key = Key;

                    std::optional<int64> Scancode;
                    if (!ReadInteger(t_Line, "scancode", Scancode, Error, 0, std::numeric_limits<uint32>::max()))
                    {
                        return Error;
                    }
                    Message.Recorded.Message.Scancode = static_cast<uint32>(Scancode.value_or(0));
                }

                // Device
                std::string DeviceError;
                const std::optional<uint32> Device =
                    ResolveDevice(t_Line, Type, Message.Recorded.Message.Key, DeviceError);
                if (!Device)
                {
                    return DeviceError;
                }
                Message.Recorded.Message.Device = InputDeviceId{*Device};

                std::vector<ParsedMessage> Result;
                switch (Type)
                {
                case InputMessageType::Axis:
                {
                    std::optional<double> Value;
                    if (!ReadNumber(t_Line, "value", Value, Error))
                    {
                        return Error;
                    }
                    if (!Value)
                    {
                        return "missing \"value\"";
                    }
                    Message.Recorded.Message.Value = static_cast<float>(*Value);
                    break;
                }
                case InputMessageType::MouseMove:
                {
                    std::optional<int64> Dx;
                    std::optional<int64> Dy;
                    std::optional<double> X;
                    std::optional<double> Y;
                    constexpr int64 Max = std::numeric_limits<int32>::max();
                    if (!ReadInteger(t_Line, "dx", Dx, Error, -Max, Max) ||
                        !ReadInteger(t_Line, "dy", Dy, Error, -Max, Max) || !ReadNumber(t_Line, "x", X, Error) ||
                        !ReadNumber(t_Line, "y", Y, Error))
                    {
                        return Error;
                    }
                    InputMessage& Move = Message.Recorded.Message;
                    Move.DeltaX = static_cast<int32>(Dx.value_or(0));
                    Move.DeltaY = static_cast<int32>(Dy.value_or(0));
                    Move.PositionX = static_cast<float>(X.value_or(0.0));
                    Move.PositionY = static_cast<float>(Y.value_or(0.0));
                    Message.NeedsX = !X;
                    Message.NeedsY = !Y;
                    break;
                }
                case InputMessageType::MouseWheel:
                {
                    std::optional<double> Wx;
                    std::optional<double> Wy;
                    if (!ReadNumber(t_Line, "wx", Wx, Error) || !ReadNumber(t_Line, "wy", Wy, Error))
                    {
                        return Error;
                    }
                    Message.Recorded.Message.WheelX = static_cast<float>(Wx.value_or(0.0));
                    Message.Recorded.Message.WheelY = static_cast<float>(Wy.value_or(0.0));
                    break;
                }
                case InputMessageType::Char:
                {
                    std::vector<uint32> CodePoints;
                    const auto Text = t_Line.find("text");
                    std::optional<int64> CodePoint;
                    if (!ReadInteger(t_Line, "codepoint", CodePoint, Error, 0, 0x10FFFF))
                    {
                        return Error;
                    }
                    if (CodePoint)
                    {
                        CodePoints.push_back(static_cast<uint32>(*CodePoint));
                    }
                    else if (Text != t_Line.end())
                    {
                        if (!Text->is_string() || !DecodeUtf8(Text->get<std::string>(), CodePoints))
                        {
                            return "\"text\" must be a string of valid UTF-8";
                        }
                    }
                    else
                    {
                        return "a Char message needs \"codepoint\" or \"text\"";
                    }
                    for (const uint32 Value : CodePoints)
                    {
                        ParsedMessage Item = Message;
                        Item.Recorded.Message.CodePoint = Value;
                        m_Messages.push_back(std::move(Item));
                    }
                    return {};
                }
                default:
                    break;
                }

                if (IsPress)
                {
                    std::optional<double> Duration;
                    std::optional<int64> Ticks;
                    if (!ReadNumber(t_Line, "duration", Duration, Error) ||
                        !ReadInteger(t_Line, "ticks", Ticks, Error, 0, std::numeric_limits<int64>::max()))
                    {
                        return Error;
                    }
                    double Hold = 0.0;
                    if (Duration)
                    {
                        if (*Duration < 0.0)
                        {
                            return "\"duration\" must not be negative";
                        }
                        Hold = *Duration;
                    }
                    else if (Ticks)
                    {
                        Hold = static_cast<double>(*Ticks) / static_cast<double>(m_Out.TickRate);
                    }
                    ParsedMessage Down = Message;
                    Down.Recorded.Message.Type = InputMessageType::KeyDown;
                    ParsedMessage Up = Message;
                    Up.Recorded.Message.Type = InputMessageType::KeyUp;
                    Up.Recorded.TimeSeconds = Time + Hold;
                    m_Messages.push_back(std::move(Down));
                    m_Messages.push_back(std::move(Up));
                    return {};
                }

                m_Messages.push_back(std::move(Message));
                return {};
            }

            std::optional<uint32>
            ResolveDevice(const Json& t_Line, InputMessageType t_Type, InputKey t_Key, std::string& t_Error)
            {
                const auto Field = t_Line.find("device");
                if (Field != t_Line.end())
                {
                    if (Field->is_number_unsigned())
                    {
                        const uint64 Id = Field->get<uint64>();
                        if (Id > std::numeric_limits<uint32>::max() ||
                            m_Out.FindDevice(static_cast<uint32>(Id)) == nullptr)
                        {
                            t_Error = "device " + std::to_string(Id) + " is not in the header";
                            return std::nullopt;
                        }
                        return static_cast<uint32>(Id);
                    }
                    if (Field->is_string())
                    {
                        InputDeviceType Type = InputDeviceType::Unknown;
                        if (!ParseDeviceType(Field->get<std::string>(), Type))
                        {
                            t_Error = "unknown device type " + Quote(Field->get<std::string>());
                            return std::nullopt;
                        }
                        return FindOrAddDevice(Type);
                    }
                    t_Error = "\"device\" must be an id or a device type name";
                    return std::nullopt;
                }

                InputDeviceType Type = InputDeviceType::Keyboard;
                switch (t_Type)
                {
                case InputMessageType::MouseMove:
                case InputMessageType::MouseWheel:
                    Type = InputDeviceType::Mouse;
                    break;
                case InputMessageType::Char:
                case InputMessageType::FocusLost:
                case InputMessageType::FocusGained:
                    Type = InputDeviceType::Keyboard;
                    break;
                default:
                    if (t_Key.IsKeyboardKey())
                    {
                        Type = InputDeviceType::Keyboard;
                    }
                    else if (t_Key.IsMouseButton())
                    {
                        Type = InputDeviceType::Mouse;
                    }
                    else if (t_Key.IsGamepadKey())
                    {
                        Type = InputDeviceType::Gamepad;
                    }
                    break;
                }
                return FindOrAddDevice(Type);
            }

            /** Sort by time, then fill in the mouse positions that the file did not give. */
            void Finish()
            {
                std::stable_sort(m_Messages.begin(),
                                 m_Messages.end(),
                                 [](const ParsedMessage& t_A, const ParsedMessage& t_B)
                                 { return t_A.Recorded.TimeSeconds < t_B.Recorded.TimeSeconds; });

                std::map<uint32, std::pair<float, float>> Positions;
                for (ParsedMessage& Item : m_Messages)
                {
                    InputMessage& Message = Item.Recorded.Message;
                    if (Message.Type == InputMessageType::MouseMove)
                    {
                        std::pair<float, float>& Position = Positions[Message.Device.Value];
                        if (Item.NeedsX)
                        {
                            Message.PositionX = Position.first + static_cast<float>(Message.DeltaX);
                        }
                        if (Item.NeedsY)
                        {
                            Message.PositionY = Position.second + static_cast<float>(Message.DeltaY);
                        }
                        Position = {Message.PositionX, Message.PositionY};
                    }
                    m_Out.Messages.push_back(std::move(Item.Recorded));
                }
                m_Messages.clear();
            }

            InputRecording& m_Out;

            std::vector<ParsedMessage> m_Messages;

            uint32 m_NextDeviceId = 1;
        };

        /** Writes one JSON object on one line with the fields in the order they are added. */
        class LineWriter
        {
        public:
            template <typename T>
            void Add(const char* t_Name, const T& t_Value)
            {
                m_Text += m_Text.empty() ? '{' : ',';
                m_Text += Json(t_Name).dump();
                m_Text += ':';
                m_Text += Json(t_Value).dump(-1, ' ', false, Json::error_handler_t::replace);
            }

            /** A float as the shortest decimal that reads back as the same float. */
            void Add(const char* t_Name, float t_Value)
            {
                char Buffer[32];
                for (int Digits = 6; Digits <= 9; ++Digits)
                {
                    std::snprintf(Buffer, sizeof(Buffer), "%.*g", Digits, static_cast<double>(t_Value));
                    if (std::strtof(Buffer, nullptr) == t_Value)
                    {
                        break;
                    }
                }
                Add(t_Name, std::strtod(Buffer, nullptr));
            }

            std::string Finish() { return m_Text + '}'; }

        private:
            std::string m_Text;
        };

        void AppendFormatted(std::string& t_Out, const char* t_Format, ...)
        {
            char Buffer[160];
            va_list Args;
            va_start(Args, t_Format);
            const int Length = std::vsnprintf(Buffer, sizeof(Buffer), t_Format, Args);
            va_end(Args);
            if (Length > 0)
            {
                t_Out.append(Buffer, std::min(static_cast<size_t>(Length), sizeof(Buffer) - 1));
            }
        }
    } // namespace

    double InputRecording::GetLength() const
    {
        double Length = Duration;
        for (const InputRecordedMessage& Message : Messages)
        {
            Length = std::max(Length, Message.TimeSeconds);
        }
        return Length;
    }

    const InputRecordingDevice* InputRecording::FindDevice(uint32 t_Id) const
    {
        for (const InputRecordingDevice& Device : Devices)
        {
            if (Device.Id == t_Id)
            {
                return &Device;
            }
        }
        return nullptr;
    }

    bool ParseInputRecording(std::string_view t_Text, InputRecording& t_Out, std::string& t_OutError)
    {
        InputRecording Result;
        RecordingParser Parser(Result);
        t_OutError = Parser.Run(t_Text);
        if (!t_OutError.empty())
        {
            return false;
        }
        t_Out = std::move(Result);
        return true;
    }

    bool LoadInputRecording(const std::string& t_Path, InputRecording& t_Out, std::string& t_OutError)
    {
        std::ifstream File(t_Path, std::ios::binary);
        if (!File)
        {
            t_OutError = "cannot open '" + t_Path + "'";
            return false;
        }
        std::ostringstream Buffer;
        Buffer << File.rdbuf();
        if (!ParseInputRecording(Buffer.str(), t_Out, t_OutError))
        {
            t_OutError = t_Path + ": " + t_OutError;
            return false;
        }
        return true;
    }

    std::string WriteInputRecording(const InputRecording& t_Recording)
    {
        std::string Text;
        LineWriter Header;
        Header.Add("format", InputRecording::FormatName);
        Header.Add("version", t_Recording.Version);
        Header.Add("tickRate", t_Recording.TickRate);
        Header.Add("duration", RoundMicroseconds(t_Recording.Duration));
        Json Devices = Json::array();
        for (const InputRecordingDevice& Device : t_Recording.Devices)
        {
            Json Capabilities = Json::array();
            for (const CapabilityName& Item : CapabilityNames)
            {
                if (Device.Info.HasCapability(Item.Capability))
                {
                    Capabilities.push_back(Item.Text);
                }
            }
            LineWriter Entry;
            Entry.Add("id", Device.Id);
            Entry.Add("type", ToString(Device.Info.Type));
            Entry.Add("name", Device.Info.DisplayName);
            Entry.Add("vendorId", Device.Info.VendorId);
            Entry.Add("productId", Device.Info.ProductId);
            Entry.Add("path", Device.Info.HardwarePath);
            Entry.Add("capabilities", Capabilities);
            Devices.push_back(Json::parse(Entry.Finish()));
        }
        Header.Add("devices", Devices);
        Text += Header.Finish();
        Text += '\n';

        for (const InputRecordedMessage& Recorded : t_Recording.Messages)
        {
            const InputMessage& Message = Recorded.Message;
            if (Message.Type == InputMessageType::DeviceConnected ||
                Message.Type == InputMessageType::DeviceDisconnected)
            {
                continue;
            }
            LineWriter Line;
            Line.Add("t", RoundMicroseconds(Recorded.TimeSeconds));
            Line.Add("device", Message.Device.Value);
            Line.Add("type", ToString(Message.Type));
            switch (Message.Type)
            {
            case InputMessageType::KeyDown:
            case InputMessageType::KeyUp:
            case InputMessageType::KeyRepeat:
                Line.Add("key", Message.Key.GetName());
                if (Message.Scancode != 0)
                {
                    Line.Add("scancode", Message.Scancode);
                }
                break;
            case InputMessageType::Char:
                Line.Add("codepoint", Message.CodePoint);
                break;
            case InputMessageType::Axis:
                Line.Add("key", Message.Key.GetName());
                Line.Add("value", Message.Value);
                break;
            case InputMessageType::MouseMove:
                Line.Add("dx", Message.DeltaX);
                Line.Add("dy", Message.DeltaY);
                Line.Add("x", Message.PositionX);
                Line.Add("y", Message.PositionY);
                break;
            case InputMessageType::MouseWheel:
                Line.Add("wx", Message.WheelX);
                Line.Add("wy", Message.WheelY);
                break;
            default:
                break;
            }
            Text += Line.Finish();
            Text += '\n';
        }
        return Text;
    }

    bool SaveInputRecording(const std::string& t_Path, const InputRecording& t_Recording, std::string& t_OutError)
    {
        namespace fs = std::filesystem;
        std::error_code Code;
        const fs::path Path(t_Path);
        if (Path.has_parent_path())
        {
            fs::create_directories(Path.parent_path(), Code);
            if (Code)
            {
                t_OutError = "cannot make folder '" + Path.parent_path().string() + "': " + Code.message();
                return false;
            }
        }
        std::ofstream File(t_Path, std::ios::binary | std::ios::trunc);
        if (!File)
        {
            t_OutError = "cannot write '" + t_Path + "'";
            return false;
        }
        File << WriteInputRecording(t_Recording);
        File.flush();
        if (!File)
        {
            t_OutError = "error while writing '" + t_Path + "'";
            return false;
        }
        return true;
    }

    std::string
    DescribeRecordedMessage(double t_TimeSeconds, std::string_view t_DeviceName, const InputMessage& t_Message)
    {
        std::string Result;
        AppendFormatted(Result, "%.3fs  ", t_TimeSeconds);
        Result += t_DeviceName.empty() ? std::string_view("Device") : t_DeviceName;
        Result += "  ";
        Result += ToString(t_Message.Type);
        switch (t_Message.Type)
        {
        case InputMessageType::KeyDown:
        case InputMessageType::KeyUp:
        case InputMessageType::KeyRepeat:
            Result += ' ';
            Result += t_Message.Key.GetName();
            break;
        case InputMessageType::Char:
            AppendFormatted(Result, " U+%04X", static_cast<unsigned>(t_Message.CodePoint));
            break;
        case InputMessageType::Axis:
            Result += ' ';
            Result += t_Message.Key.GetName();
            AppendFormatted(Result, " %.3f", static_cast<double>(t_Message.Value));
            break;
        case InputMessageType::MouseMove:
            AppendFormatted(Result, " dx=%d dy=%d", t_Message.DeltaX, t_Message.DeltaY);
            break;
        case InputMessageType::MouseWheel:
            AppendFormatted(Result,
                            " wx=%.2f wy=%.2f",
                            static_cast<double>(t_Message.WheelX),
                            static_cast<double>(t_Message.WheelY));
            break;
        default:
            break;
        }
        return Result;
    }

    void InputEventLog::Add(std::string t_Line)
    {
        m_Lines.push_back(std::move(t_Line));
        ++m_TotalAdded;
        while (m_Lines.size() > m_MaxLines)
        {
            m_Lines.pop_front();
        }
    }

    void InputEventLog::Clear()
    {
        m_Lines.clear();
        m_TotalAdded = 0;
    }
} // namespace Fling
