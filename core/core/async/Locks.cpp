//
// Created by jglrxavpok on 02/02/2022.
//

#include "Locks.h"
#include <thread>
#include <core/Macros.h>

namespace Carrot::Async {

    // SpinLock
    SpinLock::SpinLock(): acquired() {}

    void SpinLock::lock() {
        while(!tryLock()) {
            std::this_thread::yield();
        }
    }

    bool SpinLock::tryLock() {
        bool alreadyLocked = acquired.test_and_set(std::memory_order_acquire);
        return !alreadyLocked;
    }

    void SpinLock::unlock() {
        acquired.clear(std::memory_order_release);
    }

    SpinLock::~SpinLock() {
        verifyTerminate(!acquired.test(std::memory_order_acquire), "Lock should not be acquired when destroying it");
    }

    // Reentrant spin lock
    std::hash<std::thread::id> ReentrantSpinLock::threadIDHasher;

    void ReentrantSpinLock::lock() {
        std::size_t currentThreadID = threadIDHasher(std::this_thread::get_id());

        if(owningThreadHash.load(std::memory_order_relaxed) != currentThreadID) {
            std::size_t unlockValue = 0;

            while(!owningThreadHash.compare_exchange_weak(unlockValue, currentThreadID, std::memory_order_relaxed, std::memory_order_relaxed)) {
                unlockValue = 0; // reset unlock value
                std::this_thread::yield();
            }
        }

        reentryCount++;

        std::atomic_thread_fence(std::memory_order_acquire);
    }

    void ReentrantSpinLock::unlock() {
        std::atomic_thread_fence(std::memory_order_acquire);

        std::size_t currentThreadID = threadIDHasher(std::this_thread::get_id());
        std::size_t owningThreadID = owningThreadHash.load(std::memory_order_relaxed);
        verify(owningThreadID == currentThreadID, "Lock is not owned by the same thread?");

        verify(reentryCount > 0, "Mismatched lock/unlock calls");
        reentryCount--;
        if(reentryCount == 0) {
            owningThreadHash.store(0, std::memory_order_relaxed);
        }
    }

    bool ReentrantSpinLock::tryLock() {
        std::size_t currentThreadID = threadIDHasher(std::this_thread::get_id());

        bool acquired = true;
        if(owningThreadHash.load(std::memory_order_relaxed) != currentThreadID) { // not owned by current thread
            std::size_t unlockValue = 0;

            if(!owningThreadHash.compare_exchange_weak(unlockValue, currentThreadID, std::memory_order_relaxed, std::memory_order_relaxed)) { // locked by other thread
                acquired = false;
            }
        }

        if(acquired) {
            reentryCount++;
        }

        std::atomic_thread_fence(std::memory_order_acquire);
        return acquired;
    }

    ReentrantSpinLock::~ReentrantSpinLock() noexcept {
        verifyTerminate(reentryCount == 0, "Lock should not be acquired when destroying it");
    }

    // ReadWriteLock
    ReadWriteLock::ReadWriteLock(): writeLock(*this), readLock(*this) {}

    ReadLock& ReadWriteLock::read() {
        return readLock;
    }

    WriteLock& ReadWriteLock::write() {
        return writeLock;
    }

    ReadWriteLock::~ReadWriteLock() {
        verifyTerminate(readerCount == 0, "Lock should not be acquired when destroying it");
    }

    // ReadLock

    ReadLock::ReadLock(ReadWriteLock& parent): parent(parent) {}
    ReadLock::~ReadLock() noexcept {}

    void ReadLock::lock() {
        std::uint32_t currentReaderCount = parent.atomicValue.load();

        if(currentReaderCount == ReadWriteLock::WriterPresentValue) { // locked by writer
            std::uint32_t unlockValue = 0;
            while(!parent.atomicValue.compare_exchange_weak(unlockValue, 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
                unlockValue = 0;
                std::this_thread::yield();
            }
        } else {
            std::uint32_t nextReaderCount = currentReaderCount +1;

            while(!parent.atomicValue.compare_exchange_weak(currentReaderCount, nextReaderCount, std::memory_order_relaxed, std::memory_order_relaxed)) {
                if(currentReaderCount == ReadWriteLock::WriterPresentValue) {
                    currentReaderCount = 0;
                } else {
                    currentReaderCount = parent.atomicValue.load();
                }
                nextReaderCount = currentReaderCount + 1;
                std::this_thread::yield();
            }
        }

        parent.readerCount++;
        std::atomic_thread_fence(std::memory_order_acquire);
    }

    void ReadLock::unlock() {
        std::atomic_thread_fence(std::memory_order_acquire);

        verify(parent.readerCount > 0, "Mismatched lock/unlock");

        std::uint32_t currentReaderCount = parent.atomicValue.load();
        verify(currentReaderCount > 0 && currentReaderCount != ReadWriteLock::WriterPresentValue, "Lock acquired while a writer was still present?");

        std::uint32_t nextReaderCount = currentReaderCount -1;

        parent.readerCount--;

        while(!parent.atomicValue.compare_exchange_weak(currentReaderCount, nextReaderCount, std::memory_order_relaxed, std::memory_order_relaxed)) {
            verify(currentReaderCount != ReadWriteLock::WriterPresentValue, "Lock acquired while a writer was still present?");
            currentReaderCount = parent.atomicValue.load();
            nextReaderCount = currentReaderCount - 1;
            std::this_thread::yield();
        }

        std::atomic_thread_fence(std::memory_order_release);
    }

    bool ReadLock::tryLock() {
        std::uint32_t currentReaderCount = parent.atomicValue.load();
        std::uint32_t nextReaderCount = currentReaderCount +1;

        bool acquired = true;

        if(!parent.atomicValue.compare_exchange_weak(currentReaderCount, nextReaderCount, std::memory_order_relaxed, std::memory_order_relaxed)) {
            acquired = false;
        }

        if(acquired) {
            parent.readerCount++;
        }

        std::atomic_thread_fence(std::memory_order_acquire);
        return acquired;
    }

    // WriteLock

    WriteLock::WriteLock(ReadWriteLock& parent): parent(parent) {}
    WriteLock::~WriteLock() noexcept {}

    void WriteLock::lock() {
        std::uint32_t lockValue = 0;
        while(!parent.atomicValue.compare_exchange_weak(lockValue, ReadWriteLock::WriterPresentValue, std::memory_order_relaxed, std::memory_order_relaxed)) {
            lockValue = 0; // reset unlock value
            std::this_thread::yield();
        }

        verify(parent.readerCount == 0, "Acquired writer lock while reader was still present");

        std::atomic_thread_fence(std::memory_order_acquire);
    }

    bool WriteLock::tryLock() {
        bool acquired = true;

        std::uint32_t lockValue = 0;
        if(!parent.atomicValue.compare_exchange_weak(lockValue, ReadWriteLock::WriterPresentValue, std::memory_order_relaxed, std::memory_order_relaxed)) {
            acquired = false;
        }

        std::atomic_thread_fence(std::memory_order_acquire);
        return acquired;
    }

    void WriteLock::unlock() {
        std::atomic_thread_fence(std::memory_order_acquire);

        std::uint32_t expected = ReadWriteLock::WriterPresentValue;
        if(!parent.atomicValue.compare_exchange_weak(expected, 0, std::memory_order_relaxed, std::memory_order_relaxed)) {
            verify(true, "Lock was not held by writer");
        }

        std::atomic_thread_fence(std::memory_order_release);
    }

    // LockGuard

    LockGuard::LockGuard(BasicLock& lock): lock(lock) {
        lock.lock();
    }

    LockGuard::~LockGuard() {
        lock.unlock();
    }
}