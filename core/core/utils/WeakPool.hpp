//
// Created by jglrxavpok on 05/11/2021.
//

#pragma once

#include <cstdint>
#include <unordered_map>
#include <queue>

namespace Carrot {

    class WeakPoolHandle {
    public:
        explicit WeakPoolHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor): index(index), destructor(std::move(destructor)) {}

        std::uint32_t getSlot() const { return index; }

        virtual ~WeakPoolHandle() {
            destructor(this);
        }

    private:
        std::uint32_t index = -1;
        std::function<void(WeakPoolHandle*)> destructor;
    };

    /// Pool of weak_ptr to a given ElementType (which must be derived from WeakPoolHandle)
    ///  Getting an item out of the pool is done with ::create. The returned object (wrapped inside a shared_ptr) will
    ///  release the slot back to the pool once its destructor is called.
    ///  This pool does not store the objects, but their ID! This is used to distribute slots inside a buffer.
    /// \tparam ElementType
    template<typename ElementType> requires std::derived_from<ElementType, WeakPoolHandle>
    class WeakPool {
    public:
        using Registry = std::unordered_map<std::uint32_t, std::weak_ptr<ElementType>>;

    public:
        std::size_t size() const {
            return registry.size();
        }

        auto begin() {
            return registry.begin();
        }

        auto begin() const {
            return registry.begin();
        }

        auto end() {
            return registry.end();
        }

        auto end() const {
            return registry.end();
        }

        void erase(auto start, auto end) {
            registry.erase(start, end);
        }

    public:
        template<typename... Args>
        std::shared_ptr<ElementType> create(Args&&... args) {
            std::uint32_t slot;
            if(!freeSlots.empty()) {
                slot = freeSlots.front();
                freeSlots.pop();
            } else {
                slot = nextID++;
            }
            auto ptr = std::make_shared<ElementType>(slot, [this](WeakPoolHandle* element) {
                freeSlot(element->getSlot());
            }, std::forward<Args>(args)...);
            registry[slot] = ptr;
            return ptr;
        }

        void freeSlot(std::uint32_t slot) {
            registry.erase(slot);
            freeSlots.push(slot);
        }

        void free(ElementType& handle) {
            freeSlot(handle.getSlot());
        }

    private:
        Registry registry;
        std::queue<std::uint32_t> freeSlots;
        std::uint32_t nextID = 1;
    };
}
