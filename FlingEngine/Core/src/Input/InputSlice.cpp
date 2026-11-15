#include "Input/InputSlice.h"

namespace Fling
{
    int32 InputSlice::MouseDeltaX() const
    {
        int32 Sum = 0;
        for (const InputMouseSample& Sample : MouseSamples)
        {
            Sum += Sample.DeltaX;
        }
        return Sum;
    }

    int32 InputSlice::MouseDeltaY() const
    {
        int32 Sum = 0;
        for (const InputMouseSample& Sample : MouseSamples)
        {
            Sum += Sample.DeltaY;
        }
        return Sum;
    }

    uint32 InputSlice::PressCount(const InputKey t_Key) const
    {
        uint32 Count = 0;
        for (const InputKeyEvent& Event : KeyEvents)
        {
            if (Event.Down && Event.Key == t_Key)
            {
                ++Count;
            }
        }
        return Count;
    }

    void InputSlice::Append(const InputMessage& t_Message, const InputCapture t_Forced)
    {
        const InputCapture Capture = t_Message.Capture | t_Forced;
        if (t_Message.Type == InputMessageType::MouseMove)
        {
            if (!HasCapture(Capture, InputCapture::Mouse))
            {
                MouseSamples.push_back(InputMouseSample{t_Message.Sequence, t_Message.DeltaX, t_Message.DeltaY});
            }
            return;
        }
        if (t_Message.Type != InputMessageType::KeyDown && t_Message.Type != InputMessageType::KeyUp)
        {
            return;
        }
        if (!IsKeyCaptured(Capture, t_Message.Key))
        {
            KeyEvents.push_back(
                InputKeyEvent{t_Message.Sequence, t_Message.Key, t_Message.Type == InputMessageType::KeyDown});
        }
    }

    void InputSlice::Clear()
    {
        Cut = InputCut{};
        MouseSamples.clear();
        KeyEvents.clear();
        Overran = false;
    }
} // namespace Fling
