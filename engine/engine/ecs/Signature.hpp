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
    public:
        using IndexType = std::int8_t;
        static_assert(MAX_COMPONENTS < std::numeric_limits<IndexType>::max());

    private:
        /// Hash to ID mapping
        static std::unordered_map<ComponentID, std::size_t> hash2index;
        static std::mutex mappingAccess;

        /// Components present in this signature
        std::bitset<MAX_COMPONENTS> components;
        std::vector<IndexType> componentIndices; // map of ComponentID -> index of component when returning multiple components based on Signature (for example LoadAllEntities in C#)
        std::size_t componentCount = 0;

        /// Gets or generates the index for the given component
        template<typename Component>
        static std::size_t getIndex();

        void reindex();

    public:
        static std::size_t getIndex(ComponentID id);

        explicit Signature();

        template<typename... Components>
        void addComponents();

        template<typename Component>
        void addComponent();

        template<typename Component>
        bool hasComponent() const;

        bool hasComponent(ComponentID componentID) const;

        void addComponent(std::size_t componentID);
        void addComponentFromComponentIndex_Internal(std::size_t componentID);

        void clear();

        IndexType getComponentIndex(std::size_t componentID) const;

        std::size_t getComponentCount() const;

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
    reindex();
}

template<typename Component>
std::size_t Carrot::Signature::getIndex() {
    return getIndex(Component::getID());
}

template<typename Component>
bool Carrot::Signature::hasComponent() const {
    auto index = getIndex<Component>();
    return components[index];
}
