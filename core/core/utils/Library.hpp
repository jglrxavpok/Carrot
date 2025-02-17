//
// Created by jglrxavpok on 06/10/2021.
//

#pragma once

#include "Identifiable.h"
#include <unordered_map>
#include <core/io/Document.h>
#include <rapidjson/document.h>

namespace Carrot {

    template<typename ContainedType, typename... Param>
    class Library {
    public:
        using ID = std::string;
        using DeserialiseFunction = std::function<ContainedType(const Carrot::DocumentElement& doc, Param&&... params)>;
        using CreateNewFunction = std::function<ContainedType(Param&&... params)>;

        explicit Library() = default;

        ContainedType deserialise(const ID& id, const Carrot::DocumentElement& doc, Param... params) const {
            return deserialisers.at(id)(doc, std::forward<Param>(params)...);
        }

        ContainedType create(const ID& id, Param... params) const {
            return creationFunctions.at(id)(std::forward<Param>(params)...);
        }

        template<typename Type> requires IsIdentifiable<Type>
        void add(const DeserialiseFunction& deserialise, const CreateNewFunction& create) {
            Type::getID();
            deserialisers[Type::getStringRepresentation()] = deserialise;
            creationFunctions[Type::getStringRepresentation()] = create;
        }

        void add(const ID& id, const DeserialiseFunction& deserialise, const CreateNewFunction& create) {
            deserialisers[id] = deserialise;
            creationFunctions[id] = create;
        }

        template<typename Type> requires IsIdentifiable<Type>
        void addUniquePtrBased() {
            return add<Type>([](const Carrot::DocumentElement& doc, Param... params) {
                return std::make_unique<Type>(doc, std::forward<Param>(params)...);
            }, [](Param... params) {
                return std::make_unique<Type>(std::forward<Param>(params)...);
            });
        }

        std::vector<ID> getAllIDs() const {
            std::vector<ID> ids;
            for(const auto& [id, _] : deserialisers) {
                ids.push_back(id);
            }
            return ids;
        }

        bool has(const ID& id) const {
            return deserialisers.find(id) != deserialisers.end();
        }

        bool remove(const ID& id) {
            bool found = false;
            found |= deserialisers.erase(id) != 0;
            found |= creationFunctions.erase(id) != 0;
            return found;
        }

    private:
        std::unordered_map<ID, DeserialiseFunction> deserialisers;
        std::unordered_map<ID, CreateNewFunction> creationFunctions;
    };
}
