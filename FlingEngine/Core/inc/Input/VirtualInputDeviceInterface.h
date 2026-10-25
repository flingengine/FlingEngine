#pragma once

#include "FlingTypes.h"
#include "Input/IInputDeviceInterface.h"
#include "Input/InputDeviceTypes.h"
#include "Input/InputMessage.h"
#include <memory>
#include <string>
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

        /** CreateDevice adds this to the end of each device's display name. */
        static constexpr const char* SimulatedNameSuffix = " (Simulated)";

        /**
         * @param t_Name    The interface name. It is part of each device's identity, so two
         *                  interfaces with different names never share device IDs
         */
        explicit VirtualInputDeviceInterface(std::string t_Name = InterfaceName) : m_Name(std::move(t_Name)) {}

        const char* GetName() const override { return m_Name.c_str(); }

        bool IsSimulated() const override { return true; }

        void Init(const std::shared_ptr<InputMessageHandler>& t_Handler) override;

        void Shutdown() override;

        /** Sends every queued message. */
        void Poll(int64 t_NowNs) override;

        /**
         * Make a virtual device, or connect again a device with the same identity. Only
         * call this after the interface is added to a handler. " (Simulated)" is added to
         * the end of the display name, if it is not there yet.
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
        std::string m_Name;

        std::weak_ptr<InputMessageHandler> m_Handler;

        std::vector<InputDeviceId> m_Devices;

        std::vector<InputMessage> m_Queued;
    };
} // namespace Fling
