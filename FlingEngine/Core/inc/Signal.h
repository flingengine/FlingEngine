#pragma once

#include <algorithm>
#include <vector>

namespace Fling
{
    template <typename Signature>
    class Signal;

    template <typename Signature>
    class Sink;

    /**
     * A list of callbacks that all run when the signal is published.
     *
     * Callbacks are a free function, or a member function and the instance to call it on.
     * The signal does not own the instances. Disconnect a callback before its instance is
     * destroyed. Connect and disconnect through a Sink, so that the owner of the signal
     * can keep Publish private.
     */
    template <typename... Args>
    class Signal<void(Args...)>
    {
    public:
        /** Call every connected callback, in the order they were connected. */
        void Publish(Args... InArgs) const
        {
            for (const Slot& Each : m_Slots)
            {
                Each.Call(Each.Instance, InArgs...);
            }
        }

        size_t Size() const { return m_Slots.size(); }

        bool Empty() const { return m_Slots.empty(); }

    private:
        friend class Sink<void(Args...)>;

        using CallFn = void (*)(void*, Args...);

        struct Slot
        {
            CallFn Call = nullptr;
            void* Instance = nullptr;
            const void* Key = nullptr;

            bool operator==(const Slot& Other) const { return Key == Other.Key && Instance == Other.Instance; }
        };

        std::vector<Slot> m_Slots;
    };

    /** Connects callbacks to a Signal and disconnects them. */
    template <typename... Args>
    class Sink<void(Args...)>
    {
    public:
        explicit Sink(Signal<void(Args...)>& InSignal) : m_Signal{&InSignal} {}

        /** Connect a free function. Connecting the same function twice has no effect. */
        template <auto Fn>
        void Connect()
        {
            Add({&CallFree<Fn>, nullptr, KeyOf<Fn>()});
        }

        /** Connect a member function on Instance. Connecting the same pair twice has no effect. */
        template <auto Fn, typename T>
        void Connect(T& Instance)
        {
            Add({&CallMember<Fn, T>, &Instance, KeyOf<Fn>()});
        }

        template <auto Fn>
        void Disconnect()
        {
            Remove({nullptr, nullptr, KeyOf<Fn>()});
        }

        template <auto Fn, typename T>
        void Disconnect(T& Instance)
        {
            Remove({nullptr, &Instance, KeyOf<Fn>()});
        }

        /** Disconnect every callback on Instance. */
        template <typename T>
        void DisconnectAll(T& Instance)
        {
            auto& Slots = m_Signal->m_Slots;
            Slots.erase(std::remove_if(
                            Slots.begin(), Slots.end(), [&](const auto& Each) { return Each.Instance == &Instance; }),
                        Slots.end());
        }

    private:
        using SlotType = typename Signal<void(Args...)>::Slot;

        /** One unique address for each callback, used to find it again on disconnect. */
        template <auto Fn>
        static const void* KeyOf()
        {
            static const char Key = 0;
            return &Key;
        }

        template <auto Fn>
        static void CallFree(void*, Args... InArgs)
        {
            Fn(InArgs...);
        }

        template <auto Fn, typename T>
        static void CallMember(void* Instance, Args... InArgs)
        {
            (static_cast<T*>(Instance)->*Fn)(InArgs...);
        }

        void Add(const SlotType& NewSlot)
        {
            auto& Slots = m_Signal->m_Slots;
            if (std::find(Slots.begin(), Slots.end(), NewSlot) == Slots.end())
            {
                Slots.push_back(NewSlot);
            }
        }

        void Remove(const SlotType& Match)
        {
            auto& Slots = m_Signal->m_Slots;
            Slots.erase(std::remove(Slots.begin(), Slots.end(), Match), Slots.end());
        }

        Signal<void(Args...)>* m_Signal;
    };
} // namespace Fling
