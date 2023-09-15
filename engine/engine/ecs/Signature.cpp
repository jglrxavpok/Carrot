//
// Created by jglrxavpok on 27/02/2021.
//

#include <core/utils/Assert.h>
#include <core/Macros.h>
#include "Signature.hpp"

std::unordered_map<Carrot::ComponentID, std::size_t> Carrot::Signature::hash2index{};
std::mutex Carrot::Signature::mappingAccess{};

namespace Carrot {
    Signature::Signature() {
        componentIndices.resize(MAX_COMPONENTS, -1);
    }

    std::size_t Signature::getIndex(ComponentID id) {
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

    void Signature::reindex() {
        IndexType index = 0;
        componentCount = 0;
        for (int i = 0; i < MAX_COMPONENTS; i++) {
            if (components[i]) {
                componentIndices[i] = index++;
                componentCount++;
            } else {
                componentIndices[i] = -1;
            }
        }
    }

    bool Signature::hasComponent(ComponentID componentID) const {
        auto index = getIndex(componentID);
        return components[index];
    }

    void Signature::addComponent(size_t componentID) {
        auto index = getIndex(componentID);
        components[index] = true;
        reindex();
    }

    void Signature::addComponentFromComponentIndex_Internal(std::size_t index) {
        components[index] = true;
        reindex();
    }

    void Signature::clear() {
        components.reset();
    }

    Signature::IndexType Signature::getComponentIndex(std::size_t componentID) const {
        auto index = getIndex(componentID);
        verify(components[index], "Component is not inside signature");
        return componentIndices[index];
    }

    std::size_t Signature::getComponentCount() const {
        return componentCount;
    }
}