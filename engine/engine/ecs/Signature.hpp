//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include <unordered_map>
#include <bitset>
#include <mutex>

namespace Carrot {
    using ComponentID = std::size_t;

    constexpr int MAX_COMPONENTS = 64;

    class Signature {
    private:
        /// Hash to ID mapping
        static std::unordered_map<ComponentID, std::size_t> hash2index;
        static std::mutex mappingAccess;

        /// Components present in this signature
        std::bitset<MAX_COMPONENTS> components;

        /// Gets or generates the index for the given component
        template<typename Component>
        static std::size_t getIndex();

        static std::size_t getIndex(ComponentID id) {
            // check if component already has an index
            auto position = hash2index.find(id);
            if(position == hash2index.end()) { // no index
                std::lock_guard lk(mappingAccess);
                position = hash2index.find(id);
                if(position == hash2index.end()) { // still no index after lock
                    auto newIndex = hash2index.size();
                    hash2index[id] = newIndex;
                    return newIndex;
                }
                return position->second;
            }

            return position->second;
        }

    public:
        explicit Signature() = default;

        template<typename... Components>
        void addComponents();

        template<typename Component>
        void addComponent();

        template<typename Component>
        bool hasComponent();

        void addComponent(size_t id) {
            auto index = getIndex(id);
            components[index] = true;
        }

        Signature operator&(const Carrot::Signature& rhs) const {
            Signature result{};
            result.components = components & rhs.components;
            return result;
        }

        friend bool operator==(const Carrot::Signature& a, const Carrot::Signature& b) {
            return a.components == b.components;
        }

        ~Signature() = default;
    };
}

template<typename... Components>
void Carrot::Signature::addComponents() {
    (addComponent<Components>(), ...);
}

template<typename Component>
void Carrot::Signature::addComponent() {
    auto index = getIndex<Component>();
    components[index] = true;
}

template<typename Component>
std::size_t Carrot::Signature::getIndex() {
    return getIndex(Component::getID());
}

template<typename Component>
bool Carrot::Signature::hasComponent() {
    auto index = getIndex<Component>();
    return components[index];
}
