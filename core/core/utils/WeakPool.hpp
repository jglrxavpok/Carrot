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

        /**
         * Required minimum element count to hold all current entries as a flat buffer, with slotIndex = index in the buffer
         */
        std::size_t getRequiredStorageCount() const {
            return requiredStorageCount;
        }

    public:
        struct Reservation {
            std::uint32_t index;
            std::weak_ptr<ElementType>& ptr;
        };

        template<typename... Args>
        std::shared_ptr<ElementType> create(Args&&... args) {
            auto slot = reserveSlot();
            auto ptr = std::make_shared<ElementType>(slot.index, [this](WeakPoolHandle* element) {
                freeSlot(element->getSlot());
            }, std::forward<Args>(args)...);
            slot.ptr = ptr;
            return ptr;
        }

        Reservation reserveSlot() {
            std::uint32_t slot;
            if(!freeSlots.empty()) {
                slot = freeSlots.front();
                freeSlots.pop();
            } else {
                slot = nextID++;
            }
            requiredStorageCount = std::max(slot+1, requiredStorageCount);
            return Reservation {
                .index = slot,
                .ptr = registry[slot],
            };
        }

        void freeSlot(std::uint32_t slot) {
            registry.erase(slot);
            freeSlots.push(slot);

            std::uint32_t highestIndex = 0;
            for(const auto& [index, ptr] : registry) {
                if(auto handle = ptr.lock()) {
                    highestIndex = std::max(handle->getSlot(), highestIndex);
                }
            }
            requiredStorageCount = highestIndex+1;
        }

        void free(ElementType& handle) {
            freeSlot(handle.getSlot());
        }

        std::weak_ptr<ElementType> find(std::uint32_t slot) {
            auto iter = registry.find(slot);
            if(iter != registry.end()) {
                return iter->second;
            }
            return {};
        }

        std::weak_ptr<const ElementType> find(std::uint32_t slot) const {
            auto iter = registry.find(slot);
            if(iter != registry.end()) {
                return iter->second;
            }
            return {};
        }

    private:
        Registry registry;
        std::queue<std::uint32_t> freeSlots;
        std::uint32_t nextID = 1;
        std::uint32_t requiredStorageCount = 0;
    };
}
