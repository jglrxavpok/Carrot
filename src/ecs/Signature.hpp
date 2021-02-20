//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include <unordered_map>
#include <bitset>
#include <mutex>
#include "ecs/components/Component.h"

namespace Carrot {
    constexpr int MAX_COMPONENTS = 64;
    using ComponentID = std::size_t;

    class Signature {
    private:
        /// Hash to ID mapping
        static std::unordered_map<Carrot::ComponentID, std::size_t> hash2index;
        static std::mutex mappingAccess;

        /// Components present in this signature
        std::bitset<MAX_COMPONENTS> components;

        /// Gets or generates the index for the given component
        template<typename Component>
        std::size_t getIndex();

    public:
        explicit Signature() = default;

        template<typename... Components>
        void addComponents();

        template<typename Component>
        void addComponent();

        template<typename Component>
        bool hasComponent();

        ~Signature() = default;
    };
}

static std::unordered_map<Carrot::ComponentID, std::size_t> hash2index{};

template<typename... Components>
void Carrot::Signature::addComponents() {
    (addComponent<Components>, ...);
}

template<typename Component>
void Carrot::Signature::addComponent() {
    auto index = getIndex<Component>();
    components[index] = true;
}

template<typename Component>
std::size_t Carrot::Signature::getIndex() {
    // check if component already has an index
    auto position = hash2index.find(Component::id);
    if(position == hash2index.end()) { // no index
        std::lock_guard lk(mappingAccess);
        position = hash2index.find(Component::id);
        if(position == hash2index.end()) { // still no index after lock
            auto newIndex = hash2index.size();
            hash2index[Component::id] = newIndex;
            return newIndex;
        }
        return position->second;
    }

    return position->second;
}

template<typename Component>
bool Carrot::Signature::hasComponent() {
    auto index = getIndex<Component>();
    return components[index];
}