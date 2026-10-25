#pragma once

#include "FlingTypes.h"
#include "NonCopyable.hpp"
#include "Input/InputDevice.h"
#include "Input/InputDeviceTypes.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Fling
{
    class IInputDeviceInterface;
    class InputMessageHandler;

    /**
     * Makes InputDevices and keeps the registry of every device in the session.
     * Interfaces call CreateDevice when they find a device, and DisconnectDevice when
     * it goes away. A disconnected device stays in the registry, so the same device
     * gets its old ID when it connects again.
     *
     * Two devices are the same device if their interface name, type, vendor ID,
     * product ID, and hardware path are all equal.
     */
    class InputDeviceFactory : public NonCopyable
    {
    public:
        explicit InputDeviceFactory(InputMessageHandler& t_Handler);

        /**
         * Make a connected device, or connect again a disconnected device with the same
         * identity. Sends a DeviceConnected message and queues a deferred connect event.
         * If a connected device with the same identity exists, it is returned unchanged.
         */
        std::shared_ptr<InputDevice> CreateDevice(const InputDeviceInfo& t_Info, const IInputDeviceInterface& t_Owner);

        /**
         * Disconnect a device. Releases its held keys first (so no consumer sees a stuck
         * key), then sends a DeviceDisconnected message and queues a deferred disconnect
         * event. Does nothing if the device is not connected.
         */
        void DisconnectDevice(InputDeviceId t_Id);

        /** Disconnect every device that t_Owner owns. */
        void DisconnectAllFrom(InputInterfaceId t_Owner);

        /** The device with this ID, or null. */
        std::shared_ptr<InputDevice> Find(InputDeviceId t_Id) const;

        /** Every device in the registry, connected or not, in ID order. */
        std::vector<std::shared_ptr<InputDevice>> GetDevices() const;

        /** Every connected device, in ID order. */
        std::vector<std::shared_ptr<InputDevice>> GetConnectedDevices() const;

    private:
        struct Record
        {
            std::string InterfaceName;
            std::shared_ptr<InputDevice> Device;
        };

        /** The handler owns the factory, so the factory never outlives it. */
        InputMessageHandler& m_Handler;

        std::map<InputDeviceId, Record> m_Devices;

        uint32 m_NextId = 1;
    };
} // namespace Fling
