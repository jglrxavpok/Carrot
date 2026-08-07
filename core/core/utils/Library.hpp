//
// Created by jglrxavpok on 06/10/2021.
//

#pragma once

#include "Identifiable.h"
#include <unordered_map>
#include <core/io/Document.h>
#include <rapidjson/document.h>

namespace Carrot {

    template<typename TElement>
    concept HasRewriteRules = requires(Carrot::DocumentElement& doc) {
        { TElement::applyRewriteRules(doc) };
    };

    /**
     * Mapping from string ID to functions required to instantiate an object
     * Mostly used to read systems & components from assets
     * @tparam ContainedType Base type of the object to be able to instantiate. This type can contain a
     *  `static void applyRewriteRules(Carrot::Document& doc)` function to allow to modify document before parsing.
     *  This is the way to go to support backwards compatibility
     * @tparam Param Types of params required to instantiate object
     */
    template<typename ContainedType, typename... Param>
    class Library {
    public:
        using ID = std::string;
        using DeserialiseFunction = std::function<ContainedType(const Carrot::DocumentElement& doc, Param&&... params)>;
        using CreateNewFunction = std::function<ContainedType(Param&&... params)>;
        using RewriteFunction = std::function<void(Carrot::DocumentElement& doc)>;

        explicit Library() = default;

        ContainedType deserialise(const ID& id, const Carrot::DocumentElement& doc, Param... params) const {
            auto rewriterIter = rewriteFunctions.find(id);
            if (rewriterIter == rewriteFunctions.end()) {
                return deserialisers.at(id)(doc, std::forward<Param>(params)...);
            } else {
                Carrot::DocumentElement rewrittenDoc = doc;
                rewriterIter->second(rewrittenDoc);
                return deserialisers.at(id)(rewrittenDoc, std::forward<Param>(params)...);
            }
        }

        ContainedType create(const ID& id, Param... params) const {
            return creationFunctions.at(id)(std::forward<Param>(params)...);
        }

        template<typename Type> requires IsIdentifiable<Type>
        void add(const DeserialiseFunction& deserialise, const CreateNewFunction& create) {
            Type::getID();
            deserialisers[Type::getStringRepresentation()] = deserialise;
            creationFunctions[Type::getStringRepresentation()] = create;

            if constexpr (HasRewriteRules<Type>) {
                rewriteFunctions[Type::getStringRepresentation()] = Type::applyRewriteRules;
            }
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

        template<typename Type> requires IsIdentifiable<Type>
        void addUniquePtrBasedV2() {
            return add<Type>([](const Carrot::DocumentElement& doc, Param... params) {
                std::unique_ptr<Type> pElement = std::make_unique<Type>(std::forward<Param>(params)...);
                pElement->deserialise(doc);
                return pElement;
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
        std::unordered_map<ID, RewriteFunction> rewriteFunctions;
    };
}
