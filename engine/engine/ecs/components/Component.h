//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include "core/utils/Identifiable.h"
#include <engine/ecs/EntityTypes.h>
#include <rapidjson/document.h>
#include <utility>
#include <core/utils/Library.hpp>

namespace Carrot::Render {
    struct Context;
}

namespace Carrot::ECS {

    class Entity;

    struct Component {
    public:
        using EntityRemappingFunction = std::function<Carrot::ECS::EntityID(const Carrot::ECS::EntityID&)>;

        explicit Component(Entity entity): entity(std::move(entity)) {}

        Entity& getEntity() { return entity; }
        const Entity& getEntity() const { return entity; }

        virtual const char* const getName() const = 0;

        virtual std::unique_ptr<Component> duplicate(const Entity& newOwner) const = 0;

        virtual Carrot::DocumentElement serialise() const {
            return Carrot::DocumentElement{};
        };

        /**
         * Called when duplicating entities, to ensure components of this entity (and its children) reference the newly created entities.
         * 'remap' should return the input if the input ID does NOT correspond to an entity that was duplicated
         */
        virtual void repairLinks(const EntityRemappingFunction& remap) {};

        virtual ~Component() = default;

        [[nodiscard]] virtual ComponentID getComponentTypeID() const = 0;

        /// Should this component be serialized inside scene files?
        virtual bool isSerializable() const;

    private:
        Entity entity;
    };

    template<class Self>
    struct IdentifiableComponent: public Component, Identifiable<Self> {
        explicit IdentifiableComponent(Entity entity): Component(std::move(entity)) {}

        virtual const char* const getName() const override {
            return Self::getStringRepresentation();
        }

        virtual ComponentID getComponentTypeID() const override {
            return Self::getID();
        }
    };

    class ComponentLibrary {
    private:
        using Storage = Library<std::unique_ptr<Component>, Entity>;

    public:
        using ID = Storage::ID;

        template<typename T> requires std::is_base_of_v<Component, T>
        void add() {
            storage.addUniquePtrBased<T>();
        }

        void add(const Storage::ID& id, const Storage::DeserialiseFunction& deserialiseFunc, const Storage::CreateNewFunction& createNewFunc);

        [[nodiscard]] std::unique_ptr<Component> deserialise(const Storage::ID& id, const Carrot::DocumentElement& doc, const Entity& entity) const;
        [[nodiscard]] std::unique_ptr<Component> create(const Storage::ID& id, const Entity& entity) const;
        [[nodiscard]] std::vector<std::string> getAllIDs() const;

        bool has(const Storage::ID& id) const;

        void remove(const Storage::ID& id);

    private:
        Storage storage;
    };

    ComponentLibrary& getComponentLibrary();
}
