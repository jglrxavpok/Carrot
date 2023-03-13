//
// Created by jglrxavpok on 11/03/2023.
//

#include <engine/utils/Macros.h>
#include <engine/Engine.h>
#include <core/scripting/csharp/Engine.h>
#include <core/scripting/csharp/CSArray.h>
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/CSObject.h>
#include <core/scripting/csharp/CSProperty.h>
#include <engine/ecs/Signature.hpp>
#include <mono/metadata/object.h>
#include <engine/ecs/components/CameraComponent.h>
#include <engine/ecs/components/TransformComponent.h>
#include "engine/ecs/systems/System.h"
#include "engine/ecs/systems/CSharpLogicSystem.h"

namespace Carrot {

    static Scripting::CSProperty* SystemTypeFullNameProperty = nullptr;
    static std::unordered_map<std::string, ComponentID> HardcodedComponentIDs;

    static Scripting::CSClass* EntityClass = nullptr;
    static Scripting::CSField* EntityIDField = nullptr;
    static Scripting::CSField* EntityUserPointerField = nullptr;

    static Scripting::CSClass* SystemClass = nullptr;
    static Scripting::CSField* SystemHandleField = nullptr;
    static Scripting::CSField* SystemSignatureField = nullptr;

    static Scripting::CSClass* ComponentClass = nullptr;
    static Scripting::CSField* ComponentOwnerField = nullptr;

    static Scripting::CSClass* TransformComponentClass = nullptr;

    static std::int32_t GetMaxComponentCount() {
        return Carrot::MAX_COMPONENTS;
    }

    static ComponentID getComponentIDFromTypeName(const std::string& name) {
        auto it = HardcodedComponentIDs.find(name);
        if(it != HardcodedComponentIDs.end()) {
            return it->second;
        }

        // TODO: handle non-hardcoded
        return -1;
    }

    static ComponentID GetComponentID(MonoString* str) {
        char* chars = mono_string_to_utf8(str);

        CLEANUP(mono_free(chars));
        return getComponentIDFromTypeName(chars);
    }

    static MonoArray* LoadEntities(MonoObject* systemObj) {
        Scripting::CSObject handleObj = SystemHandleField->get(Scripting::CSObject(systemObj));
        std::uint64_t handle = *((std::uint64_t*)mono_object_unbox(handleObj));
        auto* systemPtr = reinterpret_cast<ECS::CSharpLogicSystem*>(handle);

        return systemPtr->getEntityList()->toMono();
    }

    static ECS::Entity convertToEntity(MonoObject* entityMonoObj) {
        auto entityObj = Scripting::CSObject(entityMonoObj);
        ECS::EntityID entityID = *((ECS::EntityID*)mono_object_unbox(EntityIDField->get(entityObj)));
        ECS::World* pWorld = *((ECS::World**)mono_object_unbox(EntityUserPointerField->get(entityObj)));

        return pWorld->wrap(entityID);
    }

    static std::shared_ptr<Scripting::CSObject> entityToCSObject(ECS::Entity& e) {
        ECS::EntityID uuid = e.getID();
        ECS::World* worldPtr = &e.getWorld();
        void* args[2] = {
                &uuid,
                &worldPtr,
        };
        return EntityClass->newObject(args);
    }

    static MonoObject* GetComponent(MonoObject* entityMonoObj, MonoString* type) {
        ECS::Entity entity = convertToEntity(entityMonoObj);
        ComponentID componentId = GetComponentID(type);

        // TODO: avoid allocations

        if(componentId == ECS::TransformComponent::getID()) {
            void* args[1] = {
                    entityMonoObj
            };
            auto obj = TransformComponentClass->newObject(args);
            return (MonoObject*)(*obj); // assumes the GC won't trigger before it is used
        } else {
            TODO;
        }
    }

    static glm::vec3 _GetLocalPosition(MonoObject* transformComp) {
        auto ownerEntity = ComponentOwnerField->get(Scripting::CSObject(transformComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        return entity.getComponent<ECS::TransformComponent>()->localTransform.position;
    }

    static void _SetLocalPosition(MonoObject* transformComp, glm::vec3 value) {
        auto ownerEntity = ComponentOwnerField->get(Scripting::CSObject(transformComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        entity.getComponent<ECS::TransformComponent>()->localTransform.position = value;
    }

    static MonoString* GetName(MonoObject* entityMonoObj) {
        auto entity = convertToEntity(entityMonoObj);
        return mono_string_new_wrapper(entity.getName().c_str());
    }

    void registerCSBindings(Scripting::ScriptingEngine& engine) {
        {
            HardcodedComponentIDs.clear();
            HardcodedComponentIDs["Carrot.TransformComponent"] = ECS::TransformComponent::getID();
            HardcodedComponentIDs["Carrot.CameraComponent"] = ECS::CameraComponent::getID();
        }

        {
            EntityClass = engine.findClass("Carrot", "Entity");
            verify(EntityClass, "Missing Carrot.Entity class in Carrot.dll !");
            EntityIDField = EntityClass->findField("_id");
            verify(EntityIDField, "Missing Carrot.Entity::_id field in Carrot.dll !");
            EntityUserPointerField = EntityClass->findField("_userPointer");
            verify(EntityUserPointerField, "Missing Carrot.Entity::_userPointer field in Carrot.dll !");
        }

        {
            SystemClass = engine.findClass("Carrot", "System");
            verify(SystemClass, "Missing Carrot.System class in Carrot.dll !");
            SystemHandleField = SystemClass->findField("_handle");
            verify(SystemHandleField, "Missing Carrot.System::_handle field in Carrot.dll !");
            SystemSignatureField = SystemClass->findField("_signature");
            verify(SystemSignatureField, "Missing Carrot.System::_signature field in Carrot.dll !");
        }

        {
            ComponentClass = engine.findClass("Carrot", "IComponent");
            verify(ComponentClass, "Missing Carrot.IComponent class in Carrot.dll !");
            ComponentOwnerField = ComponentClass->findField("owner");
            verify(ComponentOwnerField, "Missing Carrot.IComponent::owner field in Carrot.dll !");
        }

        {
            TransformComponentClass = engine.findClass("Carrot", "TransformComponent");
            verify(TransformComponentClass, "Missing Carrot.TransformComponent class in Carrot.dll !");
        }

        auto* typeClass = engine.findClass("System", "Type");
        verify(typeClass, "Something is very wrong!");
        SystemTypeFullNameProperty = typeClass->findProperty("FullName");

        mono_add_internal_call("Carrot.Utilities::GetMaxComponentCount", GetMaxComponentCount);
        mono_add_internal_call("Carrot.Signature::GetComponentID", GetComponentID);
        mono_add_internal_call("Carrot.System::LoadEntities", LoadEntities);
        mono_add_internal_call("Carrot.Entity::GetComponent", GetComponent);
        mono_add_internal_call("Carrot.Entity::GetName", GetName);
        mono_add_internal_call("Carrot.Entity::GetChildren", nullptr); // TODO
        mono_add_internal_call("Carrot.Entity::GetParent", nullptr); // TODO
        mono_add_internal_call("Carrot.TransformComponent::_GetLocalPosition", _GetLocalPosition);
        mono_add_internal_call("Carrot.TransformComponent::_SetLocalPosition", _SetLocalPosition);
    }
}