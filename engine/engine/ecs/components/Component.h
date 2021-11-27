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
        explicit Component(Entity entity): entity(std::move(entity)) {}

        Entity& getEntity() { return entity; }
        const Entity& getEntity() const { return entity; }

        void drawInspector(const Carrot::Render::Context& renderContext, bool& shouldKeep, bool& modified);
        virtual void drawInspectorInternals(const Carrot::Render::Context& renderContext, bool& modified) {};

        virtual const char* const getName() const = 0;

        virtual std::unique_ptr<Component> duplicate(const Entity& newOwner) const = 0;

        virtual rapidjson::Value toJSON(rapidjson::Document& doc) const {
            return rapidjson::Value(rapidjson::kObjectType);
        };

        virtual ~Component() = default;

        [[nodiscard]] virtual ComponentID getComponentTypeID() const = 0;

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

    using ComponentLibrary = Library<std::unique_ptr<Component>, Entity>;

    ComponentLibrary& getComponentLibrary();
}
