#pragma once

#include "FlingTypes.h"
#include "Threading/CpuRelax.h"
#include "Threading/ThreadSafety.h"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>

namespace Fling
{
    class ConditionVariable;

    namespace Detail
    {
        /**
         * A shared mutex made of a std::mutex and two condition variables. It has
         * the member functions of std::shared_mutex, so SharedMutex can use it in
         * place of std::shared_mutex.
         *
         * SharedMutex uses it on MinGW. There, std::shared_mutex is a winpthreads
         * rwlock, and CI saw two writers inside it at the same time (the
         * "SharedMutex lets one writer in at a time" test counted 799999 of
         * 800000 increments). The tests also run this type directly on every
         * platform.
         *
         * A writer that waits goes first: new readers wait until it has had the
         * lock, so readers cannot keep a writer out for ever.
         */
        class CondVarSharedMutex
        {
        public:
            CondVarSharedMutex() = default;

            CondVarSharedMutex(const CondVarSharedMutex&) = delete;
            CondVarSharedMutex& operator=(const CondVarSharedMutex&) = delete;

            void lock()
            {
                std::unique_lock<std::mutex> Lock(m_Mutex);
                ++m_WaitingWriters;
                m_WriterGate.wait(Lock, [this]() { return !m_Writer && m_Readers == 0; });
                --m_WaitingWriters;
                m_Writer = true;
            }

            bool try_lock()
            {
                std::lock_guard<std::mutex> Lock(m_Mutex);
                if (m_Writer || m_Readers != 0)
                {
                    return false;
                }
                m_Writer = true;
                return true;
            }

            void unlock()
            {
                // Notify while the mutex is held, so a thread that takes the lock
                // and then destroys this object cannot race with the notify
                std::lock_guard<std::mutex> Lock(m_Mutex);
                m_Writer = false;
                if (m_WaitingWriters != 0)
                {
                    m_WriterGate.notify_one();
                }
                m_ReaderGate.notify_all();
            }

            void lock_shared()
            {
                std::unique_lock<std::mutex> Lock(m_Mutex);
                m_ReaderGate.wait(Lock, [this]() { return !m_Writer && m_WaitingWriters == 0; });
                ++m_Readers;
            }

            bool try_lock_shared()
            {
                std::lock_guard<std::mutex> Lock(m_Mutex);
                if (m_Writer || m_WaitingWriters != 0)
                {
                    return false;
                }
                ++m_Readers;
                return true;
            }

            void unlock_shared()
            {
                std::lock_guard<std::mutex> Lock(m_Mutex);
                --m_Readers;
                if (m_Readers == 0 && m_WaitingWriters != 0)
                {
                    m_WriterGate.notify_one();
                }
            }

        private:
            std::mutex m_Mutex;
            std::condition_variable m_WriterGate;
            std::condition_variable m_ReaderGate;
            uint32 m_Readers = 0;
            uint32 m_WaitingWriters = 0;
            bool m_Writer = false;
        };

#if defined(__MINGW32__)
        using SharedMutexImpl = CondVarSharedMutex;
#else
        using SharedMutexImpl = std::shared_mutex;
#endif
    } // namespace Detail

    /**
     * Exclusive lock. Clang -Wthread-safety can see this type.
     *
     * Hold it with ScopedLock. Mark the data it protects with FLING_GUARDED_BY.
     */
    class FLING_CAPABILITY("mutex") Mutex
    {
    public:
        Mutex() = default;

        Mutex(const Mutex&) = delete;
        Mutex& operator=(const Mutex&) = delete;
        Mutex(Mutex&&) = delete;
        Mutex& operator=(Mutex&&) = delete;

        void Lock() FLING_ACQUIRE() { m_Mutex.lock(); }

        void Unlock() FLING_RELEASE() { m_Mutex.unlock(); }

        /** Return true when this call takes the lock. */
        bool TryLock() FLING_TRY_ACQUIRE(true) { return m_Mutex.try_lock(); }

    private:
        friend class ConditionVariable;

        std::mutex m_Mutex;
    };

    /**
     * A lock that many readers can hold, or that one writer can hold.
     *
     * Hold a read with ReadScopedLock. Hold a write with WriteScopedLock.
     */
    class FLING_CAPABILITY("shared_mutex") SharedMutex
    {
    public:
        SharedMutex() = default;

        SharedMutex(const SharedMutex&) = delete;
        SharedMutex& operator=(const SharedMutex&) = delete;
        SharedMutex(SharedMutex&&) = delete;
        SharedMutex& operator=(SharedMutex&&) = delete;

        void Lock() FLING_ACQUIRE() { m_Mutex.lock(); }

        void Unlock() FLING_RELEASE() { m_Mutex.unlock(); }

        /** Return true when this call takes the exclusive lock. */
        bool TryLock() FLING_TRY_ACQUIRE(true) { return m_Mutex.try_lock(); }

        void LockShared() FLING_ACQUIRE_SHARED() { m_Mutex.lock_shared(); }

        void UnlockShared() FLING_RELEASE_SHARED() { m_Mutex.unlock_shared(); }

        /** Return true when this call takes a shared lock. */
        bool TryLockShared() FLING_TRY_ACQUIRE_SHARED(true) { return m_Mutex.try_lock_shared(); }

    private:
        // std::shared_mutex, except on MinGW (see Detail::CondVarSharedMutex)
        Detail::SharedMutexImpl m_Mutex;
    };

    /** Holds a shared lock on a SharedMutex until it is destroyed. */
    class FLING_SCOPED_CAPABILITY ReadScopedLock
    {
    public:
        explicit ReadScopedLock(SharedMutex& t_Mutex) FLING_ACQUIRE_SHARED(t_Mutex) : m_Mutex(t_Mutex), m_Locked(true)
        {
            m_Mutex.LockShared();
        }

        // FLING_RELEASE, not FLING_RELEASE_SHARED. The constructor acquired the
        // shared capability. Clang warns if the scoped destructor uses the shared form.
        ~ReadScopedLock() FLING_RELEASE()
        {
            if (m_Locked)
            {
                m_Mutex.UnlockShared();
            }
        }

        ReadScopedLock(const ReadScopedLock&) = delete;
        ReadScopedLock& operator=(const ReadScopedLock&) = delete;
        ReadScopedLock(ReadScopedLock&&) = delete;
        ReadScopedLock& operator=(ReadScopedLock&&) = delete;

        /** Release the shared lock before the ReadScopedLock is destroyed. */
        void Unlock() FLING_RELEASE()
        {
            assert(m_Locked);
            m_Mutex.UnlockShared();
            m_Locked = false;
        }

        /** Take the shared lock again after Unlock. */
        void Lock() FLING_ACQUIRE_SHARED()
        {
            assert(!m_Locked);
            m_Mutex.LockShared();
            m_Locked = true;
        }

    private:
        SharedMutex& m_Mutex;
        bool m_Locked = false;
    };

    /**
     * A lock for a critical section of a few instructions.
     *
     * Do not hold a SpinLock across an allocation, IO, or a wait. Use Mutex
     * for those sections. A waiting thread spins, so a long section uses a core.
     */
    class FLING_CAPABILITY("spinlock") SpinLock
    {
    public:
        SpinLock() = default;

        SpinLock(const SpinLock&) = delete;
        SpinLock& operator=(const SpinLock&) = delete;
        SpinLock(SpinLock&&) = delete;
        SpinLock& operator=(SpinLock&&) = delete;

        void Lock() FLING_ACQUIRE()
        {
            while (m_Flag.test_and_set(std::memory_order_acquire))
            {
                while (m_Flag.test(std::memory_order_relaxed))
                {
                    CpuRelax();
                }
            }
        }

        void Unlock() FLING_RELEASE() { m_Flag.clear(std::memory_order_release); }

        /** Return true when this call takes the lock. */
        bool TryLock() FLING_TRY_ACQUIRE(true) { return !m_Flag.test_and_set(std::memory_order_acquire); }

    private:
        std::atomic_flag m_Flag{};
    };

    /**
     * Locks a T_Lock (Mutex, SpinLock, or the exclusive side of SharedMutex)
     * when constructed and unlocks it when destroyed.
     *
     * Unlock releases the lock early. Lock takes it again. Use the aliases
     * ScopedLock, WriteScopedLock and ScopedSpinLock.
     */
    template <typename T_Lock>
    class FLING_SCOPED_CAPABILITY BasicScopedLock
    {
    public:
        explicit BasicScopedLock(T_Lock& t_Lock) FLING_ACQUIRE(t_Lock) : m_Lock(t_Lock), m_Locked(true)
        {
            m_Lock.Lock();
        }

        ~BasicScopedLock() FLING_RELEASE()
        {
            if (m_Locked)
            {
                m_Lock.Unlock();
            }
        }

        BasicScopedLock(const BasicScopedLock&) = delete;
        BasicScopedLock& operator=(const BasicScopedLock&) = delete;
        BasicScopedLock(BasicScopedLock&&) = delete;
        BasicScopedLock& operator=(BasicScopedLock&&) = delete;

        /** Release the lock before the scoped lock is destroyed. */
        void Unlock() FLING_RELEASE()
        {
            assert(m_Locked);
            m_Lock.Unlock();
            m_Locked = false;
        }

        /** Take the lock again after Unlock. */
        void Lock() FLING_ACQUIRE()
        {
            assert(!m_Locked);
            m_Lock.Lock();
            m_Locked = true;
        }

    private:
        friend class ConditionVariable;

        T_Lock& m_Lock;
        bool m_Locked = false;
    };

    /** Holds a Mutex until it is destroyed. ConditionVariable waits take this type. */
    using ScopedLock = BasicScopedLock<Mutex>;

    /** Holds the exclusive lock on a SharedMutex until it is destroyed. */
    using WriteScopedLock = BasicScopedLock<SharedMutex>;

    /** Holds a SpinLock until it is destroyed. */
    using ScopedSpinLock = BasicScopedLock<SpinLock>;
} // namespace Fling
