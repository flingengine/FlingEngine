#pragma once

#include "Threading/CpuRelax.h"
#include "Threading/ThreadSafety.h"

#include <atomic>
#include <cassert>
#include <mutex>
#include <shared_mutex>

namespace Fling
{
    class ConditionVariable;

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
     * Locks a Mutex when constructed and unlocks it when destroyed.
     *
     * Unlock releases the mutex early. Lock takes it again.
     */
    class FLING_SCOPED_CAPABILITY ScopedLock
    {
    public:
        explicit ScopedLock(Mutex& t_Mutex) FLING_ACQUIRE(t_Mutex) : m_Mutex(t_Mutex), m_Locked(true)
        {
            m_Mutex.Lock();
        }

        ~ScopedLock() FLING_RELEASE()
        {
            if (m_Locked)
            {
                m_Mutex.Unlock();
            }
        }

        ScopedLock(const ScopedLock&) = delete;
        ScopedLock& operator=(const ScopedLock&) = delete;
        ScopedLock(ScopedLock&&) = delete;
        ScopedLock& operator=(ScopedLock&&) = delete;

        /** Release the mutex before the ScopedLock is destroyed. */
        void Unlock() FLING_RELEASE()
        {
            assert(m_Locked);
            m_Mutex.Unlock();
            m_Locked = false;
        }

        /** Take the mutex again after Unlock. */
        void Lock() FLING_ACQUIRE()
        {
            assert(!m_Locked);
            m_Mutex.Lock();
            m_Locked = true;
        }

    private:
        friend class ConditionVariable;

        Mutex& m_Mutex;
        bool m_Locked = false;
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
        std::shared_mutex m_Mutex;
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

    /** Holds the exclusive lock on a SharedMutex until it is destroyed. */
    class FLING_SCOPED_CAPABILITY WriteScopedLock
    {
    public:
        explicit WriteScopedLock(SharedMutex& t_Mutex) FLING_ACQUIRE(t_Mutex) : m_Mutex(t_Mutex), m_Locked(true)
        {
            m_Mutex.Lock();
        }

        ~WriteScopedLock() FLING_RELEASE()
        {
            if (m_Locked)
            {
                m_Mutex.Unlock();
            }
        }

        WriteScopedLock(const WriteScopedLock&) = delete;
        WriteScopedLock& operator=(const WriteScopedLock&) = delete;
        WriteScopedLock(WriteScopedLock&&) = delete;
        WriteScopedLock& operator=(WriteScopedLock&&) = delete;

        /** Release the exclusive lock before the WriteScopedLock is destroyed. */
        void Unlock() FLING_RELEASE()
        {
            assert(m_Locked);
            m_Mutex.Unlock();
            m_Locked = false;
        }

        /** Take the exclusive lock again after Unlock. */
        void Lock() FLING_ACQUIRE()
        {
            assert(!m_Locked);
            m_Mutex.Lock();
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

    /** Locks a SpinLock when constructed and unlocks it when destroyed. */
    class FLING_SCOPED_CAPABILITY ScopedSpinLock
    {
    public:
        explicit ScopedSpinLock(SpinLock& t_Lock) FLING_ACQUIRE(t_Lock) : m_Lock(t_Lock), m_Locked(true)
        {
            m_Lock.Lock();
        }

        ~ScopedSpinLock() FLING_RELEASE()
        {
            if (m_Locked)
            {
                m_Lock.Unlock();
            }
        }

        ScopedSpinLock(const ScopedSpinLock&) = delete;
        ScopedSpinLock& operator=(const ScopedSpinLock&) = delete;
        ScopedSpinLock(ScopedSpinLock&&) = delete;
        ScopedSpinLock& operator=(ScopedSpinLock&&) = delete;

        /** Release the spin lock before the ScopedSpinLock is destroyed. */
        void Unlock() FLING_RELEASE()
        {
            assert(m_Locked);
            m_Lock.Unlock();
            m_Locked = false;
        }

        /** Take the spin lock again after Unlock. */
        void Lock() FLING_ACQUIRE()
        {
            assert(!m_Locked);
            m_Lock.Lock();
            m_Locked = true;
        }

    private:
        SpinLock& m_Lock;
        bool m_Locked = false;
    };
} // namespace Fling
