//
// Created by jglrxavpok on 31/12/2021.
//

#pragma once

#include "Component.h"
#include <engine/physics/RigidBody.h>
#include <engine/scripting/CSharpBindings.h>

#include "core/scripting/csharp/CSObject.h"

namespace Carrot::ECS {
    struct RigidBodyComponent: IdentifiableComponent<RigidBodyComponent> {
        Physics::RigidBody rigidbody;

        explicit RigidBodyComponent(Entity entity);

        explicit RigidBodyComponent(const Carrot::DocumentElement& doc, Entity entity);

        ~RigidBodyComponent() override;

        Carrot::DocumentElement serialise() const override;

        const char *const getName() const override {
            return "RigidBodyComponent";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<RigidBodyComponent>(newOwner);
            result->rigidbody = rigidbody;
            result->wasActive = wasActive;
            result->firstTick = true;
            return result;
        }

        void reloadComponent() override;
        void unloadComponent() override;
        void dispatchEventsPostPhysicsMainThread();

        bool& getRegisteredForContactsRef();
        std::shared_ptr<Scripting::CSObject>& getCallbacksHolder();

        static const char* getTypeName(const Physics::BodyType& type);
        static Physics::BodyType getTypeFromName(const std::string& name);

    private:
        bool firstTick = true;
        bool wasActive = true;

        // scripting stuff, because C# component are transient
        Scripting::CSharpBindings::Callbacks::Handle loadCallbackHandle;
        Scripting::CSharpBindings::Callbacks::Handle unloadCallbackHandle;

        bool registeredForContacts = false;
        std::shared_ptr<Scripting::CSObject> callbacksHolder;

        friend class RigidBodySystem;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::RigidBodyComponent>::getStringRepresentation() {
    return "RigidBodyComponent";
}