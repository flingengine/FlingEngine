#pragma once

#include "FlingTypes.h"
#include "NonCopyable.hpp"
#include "Input/IInputDeviceInterface.h"
#include "Input/InputDeviceFactory.h"
#include "Input/InputDeviceStates.h"
#include "Input/InputDeviceTypes.h"
#include "Input/InputMessage.h"
#include "Input/InputMessageProcessor.h"
#include "Input/InputMessageQueue.h"
#include <entt/signal/sigh.hpp>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace Fling
{
    /**
     * The center of the input system. It owns the input interfaces, the device
     * factory, the message queue, and the newest state of every device.
     *
     * Every input goes through HandleMessage, which gives the message its sequence
     * number and time, applies it to the newest state, and pushes it to the queue.
     * Consumers read the queue with an InputMessageProcessor.
     *
     * Interfaces can be added and removed at any time. An add or remove that is asked
     * for while the handler polls (from inside an interface) is done at the end of
     * PollInterfaces.
     *
     * A handler is always owned by a shared_ptr (make one with Create), so devices,
     * interfaces, and processors can keep a weak_ptr to it and to its queue.
     */
    class InputMessageHandler : public NonCopyable, public std::enable_shared_from_this<InputMessageHandler>
    {
        /** Only Create can make this, so a handler is never on the stack or in a unique_ptr. */
        struct CreateKey
        {
            explicit CreateKey() = default;
        };

    public:
        static std::shared_ptr<InputMessageHandler> Create(uint32 t_QueueCapacity = InputMessageQueue::DefaultCapacity);

        /** Use Create. */
        InputMessageHandler(CreateKey t_Key, uint32 t_QueueCapacity);

        /** Removes every interface. */
        ~InputMessageHandler();

        /**
         * Add an interface. Calls Init, then OnWindowCreated if a window exists.
         *
         * @return The ID of the interface, for RemoveInterface
         */
        InputInterfaceId AddInterface(std::shared_ptr<IInputDeviceInterface> t_Interface);

        template <typename T, typename... Args>
        InputInterfaceId AddInterface(Args&&... t_Args)
        {
            return AddInterface(std::make_shared<T>(std::forward<Args>(t_Args)...));
        }

        /**
         * Remove an interface: calls Shutdown, disconnects all of its devices (their held
         * keys are released first), then releases the handler's reference to it. It is
         * destroyed when no other shared_ptr to it is left.
         *
         * @return False if there is no interface with this ID
         */
        bool RemoveInterface(InputInterfaceId t_Id);

        /** Remove every interface, newest first. */
        void RemoveAllInterfaces();

        /** The interface with this ID, or null. An interface that is waiting to be removed is still found. */
        std::shared_ptr<IInputDeviceInterface> FindInterface(InputInterfaceId t_Id) const;

        /** The interface with this ID as a T, or null if there is none or it is not a T. */
        template <typename T>
        std::shared_ptr<T> FindInterface(InputInterfaceId t_Id) const
        {
            return std::dynamic_pointer_cast<T>(FindInterface(t_Id));
        }

        /** The first interface with this name, or null. */
        std::shared_ptr<IInputDeviceInterface> FindInterface(std::string_view t_Name) const;

        /** Every interface that was added, in the order they were added. */
        std::vector<std::shared_ptr<IInputDeviceInterface>> GetInterfaces() const;

        /** Poll every interface that WantsPoll(t_NowNs). Then apply waiting adds and removes. */
        void PollInterfaces(int64 t_NowNs);

        /** Fire the connect and disconnect events that were queued since the last call, in order. */
        void DispatchDeferredEvents();

        /**
         * Give an input message its Sequence and TimeNs, apply it to the newest state, and
         * push it to the queue. A TimeNs of 0 means "now". A TimeNs that is earlier than
         * the last message's is set to the last message's time, so time never goes back.
         */
        void HandleMessage(InputMessage t_Message);

        /**
         * Log each message that HandleMessage gets, at this level. Off by default. Device
         * connects and disconnects are always logged, at every level.
         */
        void SetLogLevel(InputLogLevel t_Level) { m_LogLevel = t_Level; }

        InputLogLevel GetLogLevel() const { return m_LogLevel; }

        /** Release every held key and button of a device (KeyUp messages at t_TimeNs, or now if 0). */
        void ReleaseHeldKeys(InputDeviceId t_Device, int64 t_TimeNs = 0);

        /** Tell every interface about a new window. The handler keeps it for interfaces added later. */
        void NotifyWindowCreated(WindowHandle t_Window);

        void NotifyWindowDestroyed(WindowHandle t_Window);

        WindowHandle GetCurrentWindow() const { return m_Window; }

        InputDeviceFactory& GetDeviceFactory() { return m_Factory; }

        const InputDeviceFactory& GetDeviceFactory() const { return m_Factory; }

        /** Make a new consumer. It starts after the newest message, with the newest device state. */
        InputMessageProcessor CreateProcessor() const;

        const InputMessageQueue& GetQueue() const { return *m_Queue; }

        /** The newest state of every device, with every message applied. Edge counts are never cleared. */
        const InputDeviceStates& GetLatestState() const { return m_LatestState; }

        uint64 GetLatestSequence() const { return m_LastSequence; }

        /** Fired by DispatchDeferredEvents for each device that connected. */
        entt::sink<void(const InputDevice&)> OnDeviceConnected() { return entt::sink{m_OnDeviceConnected}; }

        /** Fired by DispatchDeferredEvents for each device that disconnected. */
        entt::sink<void(const InputDevice&)> OnDeviceDisconnected() { return entt::sink{m_OnDeviceDisconnected}; }

    private:
        friend class InputDeviceFactory;

        struct InterfaceEntry
        {
            InputInterfaceId Id;
            std::shared_ptr<IInputDeviceInterface> Interface;
        };

        struct DeferredDeviceEvent
        {
            InputDeviceId Device;
            bool Connected = false;
        };

        /** Called by the factory. */
        void QueueDeviceEvent(InputDeviceId t_Device, bool t_Connected);

        void AddInterfaceNow(InterfaceEntry t_Entry);

        void RemoveInterfaceNow(InputInterfaceId t_Id);

        void LogMessage(const InputMessage& t_Message) const;

        std::vector<InterfaceEntry> m_Interfaces;

        /** Adds that were asked for while polling. */
        std::vector<InterfaceEntry> m_PendingAdds;

        /** Removes that were asked for while polling. */
        std::vector<InputInterfaceId> m_PendingRemoves;

        bool m_IsPolling = false;

        uint32 m_NextInterfaceId = 1;

        InputDeviceFactory m_Factory;

        /** Shared so that processors can keep a weak_ptr to it. */
        std::shared_ptr<InputMessageQueue> m_Queue;

        InputDeviceStates m_LatestState;

        uint64 m_LastSequence = 0;

        int64 m_LastTimeNs = 0;

        std::vector<DeferredDeviceEvent> m_PendingDeviceEvents;

        WindowHandle m_Window;

        InputLogLevel m_LogLevel = InputLogLevel::Off;

        /** Log times are shown in seconds after this time. */
        int64 m_StartNs = InputNowNs();

        entt::sigh<void(const InputDevice&)> m_OnDeviceConnected;

        entt::sigh<void(const InputDevice&)> m_OnDeviceDisconnected;
    };
} // namespace Fling
