//
// Created by jglrxavpok on 09/02/2022.
//

#pragma once

#include <functional>
#include <optional>
#include <list>
#include <cider/Mutex.h>

#include "core/Macros.h"
#include "core/async/Locks.h"
#include "core/utils/Concepts.hpp"

namespace Carrot::Async {
    /// Specialized data structure that allows concurrent access to its values, via getOrCompute.
    ///  Access to different keys can be done in multiple threads, with minimal blocking.
    ///  Access to the same key can be done in multiple threads, but will block if generation started
    ///  Once created, values are never moved. Erase a value via its key and then re-set a value
    /// KeyType: must be hashable
    /// ValueType: must meet std::is_move_constructible_v
    template<typename KeyType, typename ValueType> requires Concepts::IsMoveable<ValueType> && Concepts::Hashable<KeyType>
    class ParallelMap {
        using Hash = std::size_t;

        struct Node {
            Cider::Mutex nodeAccess;

            std::optional<ValueType> value;
        };

        template<bool isConst>
        class Snapshot {
        public:
            using ElementType = std::pair<KeyType, std::conditional_t<isConst, const ValueType*, ValueType*>>;

            auto begin() {
                return keyValuePairs.begin();
            }

            auto end() {
                return keyValuePairs.end();
            }

            std::size_t size() const {
                return keyValuePairs.size();
            }

        private:
            std::vector<ElementType> keyValuePairs;
            friend class ParallelMap<KeyType, ValueType>;
        };

    public:
        using NonConstSnapshot = Snapshot<false>;
        using ConstSnapshot = Snapshot<true>;

        /// Creates an empty map
        ParallelMap() = default;

        /// Gets the value corresponding to the given key. If no such value exists, the value is created via generator.
        ValueType& getOrCompute(const KeyType& key, std::function<ValueType()> generator) {
            Node* existingNode = nullptr;
            {
                Async::LockGuard l { storageAccess.read() };
                auto iter = storage.find(key);
                if(iter != storage.end()) {
                    auto& node = iter->second;
                    if(node.value.has_value()) {
                        return node.value.value();
                    } else {
                        existingNode = &node;
                    }
                }
            }

            // Value does not exist
            {
                auto& writeLock = storageAccess.write();
                writeLock.lock();

                // node might have been created by another thread
                if(!existingNode) {
                    auto iter = storage.find(key);
                    if(iter != storage.end()) {
                        existingNode = &iter->second;
                    }
                }

                // insert empty node
                if(!existingNode) {
                    existingNode = &storage[key];
                    existingNode->value = {};
                }

                writeLock.unlock();
                Cider::BlockingMutexGuard l { existingNode->nodeAccess };

                if(existingNode->value.has_value()) {
                    return existingNode->value.value();
                }

                existingNode->value = std::move(generator());

                return existingNode->value.value();
            }
        }

        /// Sets the value corresponding to the given key. If no such value exists, the node is created.
        void replace(const KeyType& key, ValueType&& newValue) {
            {
                Async::LockGuard l { storageAccess.read() };
                auto iter = storage.find(key);
                if(iter != storage.end()) {
                    auto& node = iter->second;
                    Cider::BlockingMutexGuard l2 { node.nodeAccess };
                    node.value = std::move(newValue);
                    return;
                }
            }

            Node* existingNode = nullptr;
            auto& writeLock = storageAccess.write();
            writeLock.lock();

            // node might have been created by another thread
            auto iter = storage.find(key);
            if(iter != storage.end()) {
                existingNode = &iter->second;
            }

            // insert empty node
            if(!existingNode) {
                existingNode = &storage[key];
                existingNode->value = {};
            }

            writeLock.unlock();
            Cider::BlockingMutexGuard l { existingNode->nodeAccess };

            existingNode->value = std::move(newValue);
        }

        /// Gets the value corresponding to the given key. If no such value exists, the value is created via generator.
        ValueType& getOrCompute(Cider::FiberHandle& fiberHandle, const KeyType& key, std::function<ValueType()> generator) {
            Node* existingNode = nullptr;
            {
                Async::LockGuard l { storageAccess.read() };
                auto iter = storage.find(key);
                if(iter != storage.end()) {
                    auto& node = iter->second;
                    if(node.value.has_value()) {
                        return node.value.value();
                    } else {
                        existingNode = &node;
                    }
                }
            }

            // Value does not exist
            {
                auto& writeLock = storageAccess.write();
                writeLock.lock();

                // node might have been created by another thread
                if(!existingNode) {
                    auto iter = storage.find(key);
                    if(iter != storage.end()) {
                        existingNode = &iter->second;
                    }
                }

                // insert empty node
                if(!existingNode) {
                    existingNode = &storage[key];
                    existingNode->value = {};
                }

                writeLock.unlock();
                Cider::LockGuard l { fiberHandle, existingNode->nodeAccess };

                if(existingNode->value.has_value()) {
                    return existingNode->value.value();
                }

                existingNode->value = std::move(generator());

                return existingNode->value.value();
            }
        }

        /// Removes the value corresponding to the given key. If no such value exists, returns false. Returns true otherwise.
        bool remove(const KeyType& key) {
            Async::LockGuard l { storageAccess.write() };
            auto iter = storage.find(key);
            if(iter != storage.end()) {
                auto& node = iter->second;
                Cider::BlockingMutexGuard l1 { node.nodeAccess };
                bool result = node.value.has_value();
                node.value.reset();
                return result;
            }
            return false;
        }

        ValueType* find(const KeyType& key) {
            Async::LockGuard l{storageAccess.read()};
            auto iter = storage.find(key);
            if(iter == storage.end()) {
                return nullptr;
            }
            return &iter->second.value.value();
        }

        const ValueType* find(const KeyType& key) const {
            Async::LockGuard l { storageAccess.read() };
            auto iter = storage.find(key);
            if(iter == storage.end()) {
                return nullptr;
            }
            return &iter->second.value.value();
        }

        /// Provides a copy of this map's contents. Can be used to iterate over this structure
        NonConstSnapshot snapshot() {
            Async::LockGuard l { storageAccess.read() };

            NonConstSnapshot result;
            result.keyValuePairs.reserve(storage.size());
            for(auto& [key, node] : storage) {
                if(node.value.has_value()) {
                    result.keyValuePairs.emplace_back(key, &node.value.value());
                }
            }
            result.keyValuePairs.shrink_to_fit();

            return result;
        }

        /// Provides a copy of this map's contents. Can be used to iterate over this structure
        ConstSnapshot snapshot() const {
            Async::LockGuard l { storageAccess.read() };

            ConstSnapshot result;
            result.keyValuePairs.reserve(storage.size());
            for(auto& [key, node] : storage) {
                if(node.value.has_value()) {
                    result.keyValuePairs.emplace_back(key, &node.value.value());
                }
            }
            result.keyValuePairs.shrink_to_fit();

            return result;
        }

        void clear() {
            Async::LockGuard g { storageAccess.write() };
            for(auto& [key, node] : storage) {
                Cider::BlockingMutexGuard g2 { node.nodeAccess };
                node.value.reset();
            }
            storage.clear();
        }

    private:
        mutable Async::ReadWriteLock storageAccess{};
        std::unordered_map<KeyType, Node> storage{};
    };
}