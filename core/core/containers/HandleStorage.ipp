//
// Created by jglrxavpok on 23/11/25.
//

#pragma once

namespace Carrot {
#define HANDLE_TEMPLATE template<typename TObjectType>
//#define HANDLE_TEMPLATE template<typename TObjectType> requires CanBeUsedForHandles<TObjectType>

    HANDLE_TEMPLATE
    HandleDetails::Slot<TObjectType>::Slot(Slot&& toMove) {
        *this = std::move(toMove);
    }

    HANDLE_TEMPLATE
    HandleDetails::Slot<TObjectType>& HandleDetails::Slot<TObjectType>::operator=(HandleDetails::Slot<TObjectType>&& toMove) {
        if (this == &toMove) {
            return *this;
        }
        pObject.swap(toMove.pObject);
        toMove.pObject.reset();

        refCount = toMove.index;
        index = toMove.index;
        generationIndex = toMove.generationIndex;
        toMove.refCount = 0;
        toMove.index = 0;
        toMove.generationIndex = 0;
        return *this;
    }

    HANDLE_TEMPLATE
    void HandleDetails::Slot<TObjectType>::increaseRef() {
        refCount++;
    }

    HANDLE_TEMPLATE
    void HandleDetails::Slot<TObjectType>::decrementRef() {
        --refCount;
    }

    HANDLE_TEMPLATE
    i32 HandleDetails::Slot<TObjectType>::getRefCount() const {
        return refCount;
    }

    HANDLE_TEMPLATE
    Handle<TObjectType>::Handle(const Handle<TObjectType>& toCopy) {
        *this = toCopy;
    }

    HANDLE_TEMPLATE
    Handle<TObjectType>::Handle(Handle<TObjectType>&& toMove) noexcept {
        *this = std::move(toMove);
    }

    HANDLE_TEMPLATE
    Handle<TObjectType>::Handle(HandleDetails::Slot<TObjectType>& slot) {
        index = slot.index;
        generationIndex = slot.generationIndex;
        pStorage = slot.pStorage;
        slot.increaseRef();
    }

    HANDLE_TEMPLATE
    Handle<TObjectType>::Handle(const HandleDetails::Weak<TObjectType>& weakHandle) {
        index = weakHandle.index;
        generationIndex = weakHandle.generationIndex;
        pStorage = static_cast<HandleStorage<TObjectType>*>(weakHandle.pStorage);

        if (pStorage) {
            HandleDetails::Slot<TObjectType>* pSlot = pStorage->getSlot(index, generationIndex);
            if (pSlot) {
                pSlot->increaseRef();
            }
        }
    }

    HANDLE_TEMPLATE
    Handle<TObjectType>::~Handle() {
        clear();
    }

    HANDLE_TEMPLATE
    void Handle<TObjectType>::clear() {
        if (pStorage) {
            HandleDetails::Slot<TObjectType>* pSlot = pStorage->getSlot(index, generationIndex);
            verify(pSlot, "invalid handle");
            pSlot->decrementRef();
        }
        index = 0;
        generationIndex = 0;
        pStorage = nullptr;
    }

    HANDLE_TEMPLATE
    TObjectType& Handle<TObjectType>::operator*() const {
        TObjectType* ptr = get();
        assert(ptr);
        return *ptr;
    }

    HANDLE_TEMPLATE
    TObjectType* Handle<TObjectType>::operator->() const {
        return get();
    }

    HANDLE_TEMPLATE
    TObjectType* Handle<TObjectType>::get() const {
        if (!pStorage) {
            return nullptr;
        }

        HandleDetails::Slot<TObjectType>* pSlot = pStorage->getSlot(index, generationIndex);
        if (pSlot) {
            pSlot->increaseRef(); // TODO: multithreading?
            return &pSlot->pObject.value();
        }
        return nullptr;
    }

    HANDLE_TEMPLATE
    Handle<TObjectType>::operator bool() const {
        return get() != nullptr;
    }

    HANDLE_TEMPLATE
    Handle<TObjectType>& Handle<TObjectType>::operator=(const Handle& toCopy) {
        if (this == &toCopy) {
            return *this;
        }
        clear();
        index = toCopy.index;
        generationIndex = toCopy.generationIndex;
        pStorage = toCopy.pStorage;
        if (pStorage) {
            HandleDetails::Slot<TObjectType>* slot = pStorage->getSlot(index, generationIndex);
            slot->increaseRef(); // TODO: multithreading?
        }
        return *this;
    }

    HANDLE_TEMPLATE
    Handle<TObjectType>& Handle<TObjectType>::operator=(Handle&& toMove) noexcept {
        if (this == &toMove) {
            return *this;
        }
        clear();
        index = toMove.index;
        generationIndex = toMove.generationIndex;
        pStorage = toMove.pStorage;

        // no need to mess with references as we are moving the value
        toMove.index = 0;
        toMove.generationIndex = 0;
        toMove.pStorage = nullptr;
        return *this;
    }

    HANDLE_TEMPLATE
    HandleDetails::Slot<TObjectType>* HandleStorage<TObjectType>::getSlot(i32 index, i32 expectedGenerationIndex) {
        if(index >= storage.size()) {
            return nullptr;
        }
        HandleDetails::Slot<TObjectType>& slot = storage.at(index);
        return slot.generationIndex == expectedGenerationIndex ? &slot : nullptr;
    }

    HANDLE_TEMPLATE
    HandleDetails::Slot<TObjectType>* HandleStorage<TObjectType>::getSlot(const Handle<TObjectType>& handle) {
        return getSlot(handle.index, handle.generationIndex);
    }

    HANDLE_TEMPLATE
    HandleDetails::Slot<TObjectType>& HandleStorage<TObjectType>::allocateSlot() {
        i32 freeIndex;
        if (!freeList.empty()) {
            freeIndex = freeList[0];
            freeList.remove(0);
        } else {
            freeIndex = storage.size();
            storage.resize(freeIndex+1);
        }
        HandleDetails::Slot<TObjectType>& slot = storage[freeIndex];
        slot.index = freeIndex; // in case slot is new
        slot.generationIndex++;
        slot.pStorage = this;
        return slot;
    }

    HANDLE_TEMPLATE
    template<typename... TArgs>
    Handle<TObjectType> HandleStorage<TObjectType>::emplace(TArgs&&... args) {
        Slot& slot = allocateSlot();
        slot.pObject.emplace(args...);
        HandleDetails::Weak<TObjectType> weak;
        weak.pStorage = this;
        weak.generationIndex = slot.generationIndex;
        weak.index = slot.index;
        slot.pObject->setHandle(weak);
        return Handle<TObjectType>(slot);
    }

    HANDLE_TEMPLATE
    void HandleStorage<TObjectType>::iterate(const std::function<void(TObjectType& obj)>& func) {
        storage.iterate([&func](Slot& slot) {
            if (slot.pObject.has_value()) {
                func(slot.pObject.value());
            }
        });
    }

    HANDLE_TEMPLATE
    void HandleStorage<TObjectType>::cleanup() {
        storage.iterate([&](Slot& slot) {
            if (slot.refCount == 0) {
                slot.pObject.reset();
                freeList.pushBack(slot.index);
            }
        });
    }

}