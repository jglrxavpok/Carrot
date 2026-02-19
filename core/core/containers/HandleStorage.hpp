//
// Created by jglrxavpok on 23/11/25.
//

#pragma once

#include <core/utils/Types.h>
#include <core/containers/Vector.hpp>
#include <core/SparseArray.hpp>
#include <optional>

namespace Carrot {
    template<typename TObjectType>
    class Handle;

    namespace HandleDetails {
        template<typename TObjectType> // template arg not useful, but used to differentiate between weak references of different types
        struct Weak {
            /*HandleStorage<TObjectType>*/ void* pStorage = nullptr;
            i32 index = 0;
            i32 generationIndex = 0;
        };
    }

    /**
     * Checks if a given type is allowed for use inside handles
     */
    template<typename TObjectType>
    concept CanBeUsedForHandles = requires(TObjectType obj, HandleDetails::Weak<TObjectType> h) {
        { obj.getHandle() } -> std::convertible_to<Handle<TObjectType>>;
        { obj.setHandle(h) };
    };

    template<typename TObjectType>
    class HandleStorage;

    namespace HandleDetails {
        template<typename TObjectType>
        struct Slot {
            std::optional<TObjectType> pObject;
            HandleStorage<TObjectType>* pStorage = nullptr;
            i32 refCount = 0;
            i32 index = 0;
            i32 generationIndex = 0;

            Slot() = default;
            Slot(Slot&& toMove);
            Slot& operator=(Slot&& toMove);

            void increaseRef();
            void decrementRef();
            i32 getRefCount() const;

            friend class HandleStorage<TObjectType>;
            friend class Handle<TObjectType>;
        };
    }

    template<typename TObjectType>
    class Handle {
    public:
        Handle() = default;
        Handle(const Handle& toCopy);
        Handle(Handle&& toMove) noexcept;
        explicit Handle(HandleDetails::Slot<TObjectType>& slot);
        explicit Handle(const HandleDetails::Weak<TObjectType>& weakHandle);
        ~Handle();

        Handle& operator=(const Handle& toCopy);
        Handle& operator=(Handle&& toMove) noexcept;

        /**
         * Checks if this handle is still valid
         */
        operator bool() const;

    public: // pointer access
        TObjectType* get() const;
        TObjectType* operator->() const;
        TObjectType& operator*() const;

    public: // utility
        /**
         * Removes the reference to the underlying object from this handle. Always called on destruction
         */
        void clear();

    private:
        HandleStorage<TObjectType>* pStorage = nullptr;
        i32 index = -1;
        i32 generationIndex = 0;

        friend class HandleStorage<TObjectType>;
    };

    /**
     * Storage for objects that should be accessed via handles.
     * Handles are internally indices to a storage (managed by HandleStorage) and are:
     * - bounds-checked
     * - type-safe
     * - owned by the storage
     * - ref-counted
     *
     * Once no ref exist to a given handle, the underlying object will be deleted by the 'cleanup' function.
     * Indices inside the storage can be reused,
     *  but if by some reason you manage to get a handle that points to the same slot, its generation index will be different,
     *  meaning that the handle is considered invalid.
     *
     * HandleStorage is not thread-safe by itself, and needs external synchronisation if it can be used concurrently.
     *
     * @tparam TObjectType type of contained objects, must respect concept CanBeUsedForHandles
     */
    template<typename TObjectType>
    class HandleStorage {
    public:
        static_assert(CanBeUsedForHandles<TObjectType>);
        using Slot = HandleDetails::Slot<TObjectType>;

        /**
         * Deletes the objects inside the storage which have no ref.
         * Their memory storage can be reused later for future allocations.
         */
        void cleanup();

        /**
         * Iterates over each object currently contained in this storage, and calls 'func' on it
         */
        void iterate(const std::function<void(TObjectType& obj)>& func);

        template<typename... TArgs>
        Handle<TObjectType> emplace(TArgs&&... args);

        Slot* getSlot(i32 index, i32 generationIndex);
        Slot* getSlot(const Handle<TObjectType>& handle);

    private:
        HandleDetails::Slot<TObjectType>& allocateSlot();

        SparseArray<Slot> storage;
        Vector<i32> freeList;

        friend class Handle<TObjectType>;
        friend Slot;
    };
}

#include <core/containers/HandleStorage.ipp>