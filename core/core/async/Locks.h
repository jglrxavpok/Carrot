//
// Created by jglrxavpok on 02/02/2022.
//

#pragma once

#include <atomic>
#include <thread>

/// Implementations based on Game Engine Architecture 3rd Edition.
namespace Carrot::Async {

    class BasicLock {
    public:
        /// Acquire this lock.
        virtual void lock() = 0;

        /// Tries to acquire this lock. Returns false if failed, returns true if succeeded.
        ///  The lock is acquired after the call iif this method returns true.
        virtual bool tryLock() = 0;

        /// Releases this lock.
        virtual void unlock() = 0;

        virtual ~BasicLock() = default;
    };

    /// Basic spin-lock. Not reentrant
    class SpinLock: public BasicLock {
    public:
        SpinLock();
        ~SpinLock();

        SpinLock(const SpinLock&) = delete;
        SpinLock(SpinLock&&) = delete;
        SpinLock& operator=(const SpinLock&) = delete;
        SpinLock& operator=(SpinLock&&) = delete;

    public:
        /// Spins in a loop while trying to acquire this lock.
        virtual void lock() override;

        /// Tries to acquire this lock. Returns false if failed, returns true if succeeded.
        ///  The lock is acquired after the call iif this method returns true.
        virtual bool tryLock() override;

        /// Releases this lock.
        virtual void unlock() override;

    private:
        std::atomic_flag acquired;
    };

    /// Reentrant spin-lock
    class ReentrantSpinLock: public BasicLock {
    public:
        ReentrantSpinLock() = default;
        ~ReentrantSpinLock();

        ReentrantSpinLock(const ReentrantSpinLock&) = delete;
        ReentrantSpinLock(ReentrantSpinLock&&) = delete;
        ReentrantSpinLock& operator=(const ReentrantSpinLock&) = delete;
        ReentrantSpinLock& operator=(ReentrantSpinLock&&) = delete;

    public:
        /// Spins in a loop while trying to acquire this lock.
        virtual void lock() override;

        /// Tries to acquire this lock. Returns false if failed, returns true if succeeded.
        ///  The lock is acquired after the call iif this method returns true.
        virtual bool tryLock() override;

        /// Releases this lock.
        virtual void unlock() override;

    private:
        static std::hash<std::thread::id> threadIDHasher;

        std::atomic<std::size_t> owningThreadHash;

        // Used to ensure lock/unlock are called in pairs
        std::uint32_t reentryCount;
    };


    class ReadWriteLock;

    /// Read-part of the read-write-lock
    class ReadLock: public BasicLock {
    public:
        ~ReadLock();

        ReadLock(const ReadLock&) = delete;
        ReadLock(ReadLock&&) = delete;
        ReadLock& operator=(const ReadLock&) = delete;
        ReadLock& operator=(ReadLock&&) = delete;

    public:
        /// Spins in a loop while trying to acquire this lock.
        virtual void lock() override;

        /// Tries to acquire this lock. Returns false if failed, returns true if succeeded.
        ///  The lock is acquired after the call iif this method returns true.
        virtual bool tryLock() override;

        /// Releases this lock.
        virtual void unlock() override;

    private:
        ReadLock(ReadWriteLock& parent);

    private:
        ReadWriteLock& parent;

        friend class ReadWriteLock;
    };

    /// Write-part of the read-write-lock
    class WriteLock: public BasicLock {
    public:
        ~WriteLock();

        WriteLock(const WriteLock&) = delete;
        WriteLock(WriteLock&&) = delete;
        WriteLock& operator=(const WriteLock&) = delete;
        WriteLock& operator=(WriteLock&&) = delete;

    public:
        /// Spins in a loop while trying to acquire this lock.
        virtual void lock() override;

        /// Tries to acquire this lock. Returns false if failed, returns true if succeeded.
        ///  The lock is acquired after the call iif this method returns true.
        virtual bool tryLock() override;

        /// Releases this lock.
        virtual void unlock() override;

    private:
        explicit WriteLock(ReadWriteLock& parent);

    private:
        ReadWriteLock& parent;

        friend class ReadWriteLock;
    };

    /// Lock that supports multiple readers (shared access) and one writer (exclusive access)
    class ReadWriteLock {
    public:
        ReadWriteLock();
        ~ReadWriteLock();

        ReadLock& read();
        WriteLock& write();

        ReadWriteLock(const ReadWriteLock&) = delete;
        ReadWriteLock(ReadWriteLock&&) = delete;
        ReadWriteLock& operator=(const ReadWriteLock&) = delete;
        ReadWriteLock& operator=(ReadWriteLock&&) = delete;

    private:
        std::atomic<std::uint32_t> readerCount = 0;
        std::atomic<std::uint32_t> atomicValue;
        static constexpr std::uint32_t WriterPresentValue = std::numeric_limits<std::uint32_t>::max();
        ReadLock readLock;
        WriteLock writeLock;

        friend class ReadLock;
        friend class WriteLock;
    };

    /// RAII-construct to lock & unlock a BasicLock while is guard is alive (locks on construction, unlocks on destruction)
    class LockGuard {
    public:
        explicit LockGuard(BasicLock& lock);
        LockGuard(const LockGuard&) = delete;
        LockGuard(LockGuard&&) = default;

        ~LockGuard();

    private:
        BasicLock& lock;
    };
}