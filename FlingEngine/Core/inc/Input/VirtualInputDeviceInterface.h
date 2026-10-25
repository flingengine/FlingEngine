#pragma once

#include "FlingTypes.h"
#include "Input/IInputDeviceInterface.h"
#include "Input/InputDeviceTypes.h"
#include "Input/InputMessage.h"
#include <memory>
#include <vector>

namespace Fling
{
    /**
     * An input interface with devices that code makes: tests, input replay, agent
     * scripts, and the command layer. It needs no window, so it works with
     * --null-render. Add it to the handler when you need it, and remove it when you are
     * done.
     *
     * Queue messages for a virtual device. The next Poll sends them, in order.
     */
    class VirtualInputDeviceInterface : public IInputDeviceInterface
    {
    public:
        static constexpr const char* InterfaceName = "Virtual";

        const char* GetName() const override { return InterfaceName; }

        void Init(const std::shared_ptr<InputMessageHandler>& t_Handler) override;

        void Shutdown() override;

        /** Sends every queued message. */
        void Poll(int64 t_NowNs) override;

        /**
         * Make a virtual device, or connect again a device with the same identity. Only
         * call this after the interface is added to a handler.
         */
        InputDeviceId CreateDevice(const InputDeviceInfo& t_Info);

        /** Disconnect a device of this interface. Its held keys are released. */
        void DisconnectDevice(InputDeviceId t_Device);

        /**
         * Queue a message for the next Poll. t_Message.Device must be a device of this
         * interface; messages for other devices are dropped. Sequence is set by the
         * handler. A TimeNs of 0 means "the time of the Poll".
         */
        void Queue(const InputMessage& t_Message);

        /** The number of messages that wait for the next Poll. */
        uint32 GetQueuedCount() const { return static_cast<uint32>(m_Queued.size()); }

        const std::vector<InputDeviceId>& GetDevices() const { return m_Devices; }

    private:
        std::weak_ptr<InputMessageHandler> m_Handler;

        std::vector<InputDeviceId> m_Devices;

        std::vector<InputMessage> m_Queued;
    };
} // namespace Fling
