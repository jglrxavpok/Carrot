//
// Created by jglrxavpok on 02/02/2022.
//

#include "Locks.h"
#include <thread>
#include <core/Macros.h>

namespace Carrot::Async {

    // SpinLock
    SpinLock::SpinLock(): acquired() {
        acquired.clear();
    }

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
        verify(!acquired.test(std::memory_order_acquire), "Lock should not be acquired when destroying it");
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
        verify(reentryCount == 0, "Lock should not be acquired when destroying it");
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
        verify(readerCount == 0, "Lock should not be acquired when destroying it");
    }

    // ReadLock

    ReadLock::ReadLock(ReadWriteLock& parent): parent(parent) {}
    ReadLock::~ReadLock() noexcept {}

    void ReadLock::lock() {
#if 0
        std::uint32_t currentReaderCount = parent.atomicValue.load();

        if(currentReaderCount == ReadWriteLock::WriterPresentValue) { // locked by writer
            std::uint32_t unlockValue = 0;
            //while(!parent.atomicValue.compare_exchange_weak(unlockValue, 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
            while(!parent.atomicValue.compare_exchange_strong(unlockValue, 1)) {
                unlockValue = 0;
                std::this_thread::yield();
            }
        } else {
            std::uint32_t nextReaderCount = currentReaderCount +1;

           // while(!parent.atomicValue.compare_exchange_weak(currentReaderCount, nextReaderCount, std::memory_order_relaxed, std::memory_order_relaxed)) {
            while(!parent.atomicValue.compare_exchange_strong(currentReaderCount, nextReaderCount)) {
                if(currentReaderCount == ReadWriteLock::WriterPresentValue) {
                    currentReaderCount = 0;
                } else {
                    // what if == ReadWriteLock::WriterPresentValue after if check?
                    currentReaderCount = parent.atomicValue.load();
                }
                nextReaderCount = currentReaderCount + 1;
                std::this_thread::yield();
            }
        }

        ++parent.readerCount;
     //   std::atomic_thread_fence(std::memory_order_acquire);
#else
        parent.sharedMutex.lock_shared();
#endif
    }

    void ReadLock::unlock() {
#if 0
        //std::atomic_thread_fence(std::memory_order_acquire);

        verify(parent.readerCount > 0, "Mismatched lock/unlock");

        std::uint32_t currentReaderCount = parent.atomicValue.load();
        verify(currentReaderCount > 0, "Invalid reader count");
        //verify(currentReaderCount > 0 && currentReaderCount != ReadWriteLock::WriterPresentValue, "Lock acquired while a writer was still present?");

        std::uint32_t nextReaderCount = currentReaderCount -1;

        --parent.readerCount;

        //while(!parent.atomicValue.compare_exchange_weak(currentReaderCount, nextReaderCount, std::memory_order_relaxed, std::memory_order_relaxed)) {
        while(!parent.atomicValue.compare_exchange_strong(currentReaderCount, nextReaderCount)) {
            verify(currentReaderCount != ReadWriteLock::WriterPresentValue, "Lock acquired while a writer was still present?");
            currentReaderCount = parent.atomicValue.load();
            verify(currentReaderCount > 0, "Invalid reader count");
            nextReaderCount = currentReaderCount - 1;
            std::this_thread::yield();
        }

        //std::atomic_thread_fence(std::memory_order_release);
#else
        parent.sharedMutex.unlock_shared();
#endif
    }

    bool ReadLock::tryLock() {
#if 0
        std::uint32_t currentReaderCount = parent.atomicValue.load();
        std::uint32_t nextReaderCount = currentReaderCount +1;

        bool acquired = true;

        //if(!parent.atomicValue.compare_exchange_weak(currentReaderCount, nextReaderCount, std::memory_order_relaxed, std::memory_order_relaxed)) {
        if(!parent.atomicValue.compare_exchange_strong(currentReaderCount, nextReaderCount)) {
            acquired = false;
        }

        if(acquired) {
            parent.readerCount++;
        }

        //std::atomic_thread_fence(std::memory_order_acquire);
        return acquired;
#else
        return parent.sharedMutex.try_lock_shared();
#endif
    }

    void ReadLock::lockUpgradable() {
        parent.upgradeMutex.lock();
    }

    bool ReadLock::tryLockUpgradable() {
        return parent.upgradeMutex.try_lock();
    }

    void ReadLock::unlockUpgradable() {
        parent.upgradeMutex.unlock();
    }

    Async::WriteLock& ReadLock::upgradeToWriter() {
        lockUpgradable();
        unlock();
        parent.sharedMutex.lock();
        return parent.writeLock;
    }

    // WriteLock

    WriteLock::WriteLock(ReadWriteLock& parent): parent(parent) {}
    WriteLock::~WriteLock() noexcept {}

    void WriteLock::lock() {
#if 0
        std::uint32_t lockValue = 0;
        //while(!parent.atomicValue.compare_exchange_weak(lockValue, ReadWriteLock::WriterPresentValue, std::memory_order_relaxed, std::memory_order_relaxed)) {
        while(!parent.atomicValue.compare_exchange_strong(lockValue, ReadWriteLock::WriterPresentValue)) {
            lockValue = 0; // reset unlock value
            std::this_thread::yield();
        }

        verify(parent.readerCount == 0, "Acquired writer lock while reader was still present");

        //std::atomic_thread_fence(std::memory_order_acquire);
#else
        parent.readLock.lockUpgradable();
        parent.sharedMutex.lock();
#endif
    }

    bool WriteLock::tryLock() {
#if 0
        bool acquired = true;

        std::uint32_t lockValue = 0;
        //if(!parent.atomicValue.compare_exchange_weak(lockValue, ReadWriteLock::WriterPresentValue, std::memory_order_relaxed, std::memory_order_relaxed)) {
        if(!parent.atomicValue.compare_exchange_strong(lockValue, ReadWriteLock::WriterPresentValue)) {
            acquired = false;
        }

//        std::atomic_thread_fence(std::memory_order_acquire);
        return acquired;
#else
        bool upgradeAcquired = parent.readLock.tryLockUpgradable();
        if(!upgradeAcquired) {
            return false;
        }
        bool writerAcquired = parent.sharedMutex.try_lock();
        if(!writerAcquired) {
            parent.readLock.unlockUpgradable();
        }
        return writerAcquired;
#endif
    }

    void WriteLock::unlock() {
#if 0
        //std::atomic_thread_fence(std::memory_order_acquire);

        std::uint32_t expected = ReadWriteLock::WriterPresentValue;
        //if(!parent.atomicValue.compare_exchange_weak(expected, 0, std::memory_order_relaxed, std::memory_order_relaxed)) {
        if(!parent.atomicValue.compare_exchange_strong(expected, 0)) {
            verify(false, "Lock was not held by writer");
        }

        //std::atomic_thread_fence(std::memory_order_release);
#else
        parent.sharedMutex.unlock();
        parent.readLock.unlockUpgradable();
#endif
    }

    // LockGuard

    LockGuard::LockGuard(BasicLock& lock): lock(lock) {
        lock.lock();
    }

    LockGuard::~LockGuard() {
        lock.unlock();
    }
}