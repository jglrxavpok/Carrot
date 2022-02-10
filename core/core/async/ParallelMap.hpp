//
// Created by jglrxavpok on 09/02/2022.
//

#pragma once

#include <functional>
#include <optional>
#include <list>
#include <xhash>
#include "core/async/Locks.h"
#include "core/utils/Concepts.hpp"

namespace Carrot::Async {
    /// Specialized data structure that allows concurrent access to its values, via getOrCompute.
    ///  Access to different keys can be done in multiple threads, with minimal blocking.
    ///  Access to the same key can be done in multiple threads, but will block if generation started
    ///  Once created, values are never moved. Erasing a value via its key and then re-set a value
    /// KeyType: must be hashable
    /// ValueType: must meet std::is_move_constructible_v
    template<typename KeyType, typename ValueType> requires Concepts::IsMoveable<ValueType> && Concepts::Hashable<KeyType>
    class ParallelMap {
        using Hash = std::size_t;

        struct Node {
            Async::SpinLock nodeAccess;
            KeyType key;

            Hash hashedKey = 0;
            std::optional<ValueType> value;
        };

        class Snapshot {
        public:
            using ElementType = std::pair<KeyType, ValueType*>;

            auto begin() {
                return keyValuePairs.begin();
            }

            auto end() {
                return keyValuePairs.end();
            }

        private:
            std::vector<ElementType> keyValuePairs;
            friend class ParallelMap<KeyType, ValueType>;
        };

    public:
        /// Creates an empty map
        ParallelMap() = default;

        /// Gets the value corresponding to the given key. If no such value exists, the value is created via generator.
        ValueType& getOrCompute(const KeyType& key, std::function<ValueType()> generator) {
            Hash hashedKey = hasher(key);

            Node* existingNode = nullptr;
            {
                Async::LockGuard l { storageAccess.read() };
                for(auto& node : storage) {
                    if(node.hashedKey == hashedKey) {
                        if(node.value.has_value()) {
                            return node.value.value();
                        } else {
                            existingNode = &node;
                            break;
                        }
                    }
                }
            }

            // Value does not exist
            {
                auto& writeLock = storageAccess.write();
                writeLock.lock();

                // node might have been created by another thread
                if(!existingNode) {
                    for(auto& node : storage) {
                        if(node.hashedKey == hashedKey) {
                            existingNode = &node;
                        }
                    }
                }

                // insert empty node
                if(!existingNode) {
                    existingNode = &storage.emplace_back();
                    existingNode->key = key;
                    existingNode->hashedKey = hashedKey;
                    existingNode->value = {};
                }

                writeLock.unlock();
                Async::LockGuard l { existingNode->nodeAccess };

                if(existingNode->value.has_value()) {
                    return existingNode->value.value();
                }

                existingNode->value = std::move(generator());

                return existingNode->value.value();
            }
        }

        /// Removes the value corresponding to the given key. If no such value exists, returns false. Returns true otherwise.
        bool remove(const KeyType& key) {
            Hash hashedKey = hasher(key);
            Async::LockGuard l { storageAccess.read() };
            for(auto& node : storage) {
                if(node.hashedKey == hashedKey) {
                    Async::LockGuard l1 { node.nodeAccess };
                    bool result = node.value.has_value();
                    node.value.reset();
                    return result;
                }
            }
            return false;
        }

        /// Provides a copy of this map's contents. Can be used to iterate over this structure
        Snapshot snapshot() {
            Async::LockGuard l { storageAccess.read() };

            Snapshot result;
            result.keyValuePairs.reserve(storage.size());
            for(auto& node : storage) {
                if(node.value.has_value()) {
                    result.keyValuePairs.emplace_back(node.key, &node.value.value());
                }
            }
            result.keyValuePairs.shrink_to_fit();

            return result;
        }

    private:
        Async::ReadWriteLock storageAccess{};
        std::list<Node> storage{};
        std::hash<KeyType> hasher{};
    };
}