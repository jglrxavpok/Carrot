//
// Created by jglrxavpok on 06/10/2021.
//

#pragma once

#include "Identifiable.h"
#include <unordered_map>
#include <rapidjson/document.h>

namespace Carrot {

    template<typename ContainedType, typename... Param>
    class Library {
    public:
        using ID = std::string;
        using DeserialiseFunction = std::function<ContainedType(const rapidjson::Value& json, Param&&... params)>;

        explicit Library() = default;

        ContainedType deserialise(const ID& id, const rapidjson::Value& json, Param... params) const {
            return deserialisers.at(id)(json, std::forward<Param>(params)...);
        }

        template<typename Type> requires IsIdentifiable<Type>
        void add(const DeserialiseFunction& func) {
            deserialisers[Type::getStringRepresentation()] = func;
        }

        template<typename Type> requires IsIdentifiable<Type>
        void addUniquePtrBased() {
            return add<Type>([](const rapidjson::Value& json, Param... params) {
                return std::make_unique<Type>(json, std::forward<Param>(params)...);
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

    private:
        std::unordered_map<ID, DeserialiseFunction> deserialisers;
    };
}
