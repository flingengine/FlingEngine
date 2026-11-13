#include "Components/InputSource.h"
#include "Ecs/EntityStorage.h"
#include "Input/InputMessageHandler.h"
#include "InputUtil.h"

namespace Fling::InputUtil
{
    void Connect(EntityStorage& t_Storage, const std::shared_ptr<const InputMessageHandler>& t_Handler)
    {
        // The runner keeps a singleton that exists before it makes its own
        InputSource* Source = t_Storage.TryGetSingleton<InputSource>();
        if (Source == nullptr)
        {
            Source = &t_Storage.SetSingleton<InputSource>();
        }

        Source->Handler = t_Handler;
        Source->Reader = t_Handler ? std::make_unique<InputMessageProcessor>(t_Handler->CreateProcessor()) : nullptr;
    }

    void SetUICapture(EntityStorage& t_Storage, const bool t_Keyboard, const bool t_Mouse)
    {
        if (InputSource* Source = t_Storage.TryGetSingleton<InputSource>())
        {
            Source->KeyboardCaptured = t_Keyboard;
            Source->MouseCaptured = t_Mouse;
        }
    }
} // namespace Fling::InputUtil
