#include "CSharpComponent.h"
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/Engine.h>
#include <core/async/ParallelMap.hpp>
#include <engine/utils/Macros.h>
#include <engine/Engine.h>
#include <core/io/Logging.hpp>

#include "../../../../editor/src/Peeler.h" // TODO: this is quite ugly, find a cleaner way

namespace Carrot::ECS {

    CSharpComponent::CSharpComponent(Carrot::ECS::Entity entity, const std::string& namespaceName, const std::string& className) : Carrot::ECS::Component(
            std::move(entity)), namespaceName(namespaceName), className(className) {
        loadCallbackHandle = GetCSharpBindings().registerGameAssemblyLoadCallback([&]() { onAssemblyLoad(); });
        unloadCallbackHandle = GetCSharpBindings().registerGameAssemblyUnloadCallback([&]() { onAssemblyUnload(); });

        init();
        refresh();
    }

    CSharpComponent::CSharpComponent(const rapidjson::Value& json, Carrot::ECS::Entity entity, const std::string& namespaceName, const std::string& className) : Carrot::ECS::Component(
            std::move(entity)), namespaceName(namespaceName), className(className) {
        serializedVersion.CopyFrom(json, serializedDoc.GetAllocator());
        loadCallbackHandle = GetCSharpBindings().registerGameAssemblyLoadCallback([&]() { onAssemblyLoad(); }, true/*prepend*/); // prepend because we want components to reload before systems
        unloadCallbackHandle = GetCSharpBindings().registerGameAssemblyUnloadCallback([&]() { onAssemblyUnload(); });

        init();
        refresh();
    }

    CSharpComponent::~CSharpComponent() {
        GetCSharpBindings().unregisterGameAssemblyLoadCallback(loadCallbackHandle);
        GetCSharpBindings().unregisterGameAssemblyUnloadCallback(unloadCallbackHandle);
    }

    std::unique_ptr <Carrot::ECS::Component> CSharpComponent::duplicate(const Carrot::ECS::Entity& newOwner) const {
        serializedVersion = toJSON(serializedDoc);
        auto result = std::make_unique<CSharpComponent>(serializedVersion, newOwner, namespaceName, className);
        return result;
    }

    rapidjson::Value CSharpComponent::toJSON(rapidjson::Document& doc) const {
        if(!foundInAssemblies) { // copy last known state
            rapidjson::Value r{};
            r.CopyFrom(serializedVersion, doc.GetAllocator());
            return r;
        }

        rapidjson::Value r{rapidjson::kObjectType};

        for(auto& property : componentProperties) {
            rapidjson::Value serializationNameJSON(property.serializationName, doc.GetAllocator());
            switch(property.type) {
                case Scripting::ComponentType::Int: {
                    auto v = property.field->get(*csComponent).unbox<std::int32_t>();
                    r.AddMember(serializationNameJSON, v, doc.GetAllocator());
                } break;

                case Scripting::ComponentType::Float: {
                    auto v = property.field->get(*csComponent).unbox<float>();
                    r.AddMember(serializationNameJSON, v, doc.GetAllocator());
                } break;

                case Scripting::ComponentType::Boolean: {
                    auto v = property.field->get(*csComponent).unbox<bool>();
                    r.AddMember(serializationNameJSON, v, doc.GetAllocator());
                } break;

                case Scripting::ComponentType::Entity: {
                    MonoObject* entityObj = property.field->get(*csComponent);
                    ECS::Entity entity = GetCSharpBindings().convertToEntity(entityObj);
                    r.AddMember(serializationNameJSON, rapidjson::Value(entity.getID().toString(), doc.GetAllocator()), doc.GetAllocator());
                } break;

                case Scripting::ComponentType::UserDefined:
                default:
                    Carrot::Log::warn("Property inside %s.%s has type %s which is not supported for serialization", namespaceName.c_str(), className.c_str(), property.typeStr.c_str());
                    break;
            }
        }
        return r;
    }

    const char *const CSharpComponent::getName() const {
        return fullName.c_str();
    }

    ComponentID CSharpComponent::getComponentTypeID() const {
        return componentID;
    }

    void CSharpComponent::repairLinks(const EntityRemappingFunction& remap) {
        for (auto& property: componentProperties) {
            if(property.type != Scripting::ComponentType::Entity) {
                continue;
            }

            ECS::Entity value = GetCSharpBindings().convertToEntity(property.field->get(*csComponent));
            Entity e = getEntity().getWorld().wrap(remap(value.getID()));
            property.field->set(*csComponent, *GetCSharpBindings().entityToCSObject(e));
        }
    }

    Scripting::CSObject& CSharpComponent::getCSComponentObject() {
        verify(csComponent, "No component loaded at this point");
        return *csComponent;
    }

    bool CSharpComponent::isLoaded() {
        return csComponent != nullptr;
    }

    std::span<Scripting::ComponentProperty> CSharpComponent::getProperties() {
        return componentProperties;
    }

    void CSharpComponent::init() {
        fullName = namespaceName;
        fullName += '.';
        fullName += className;
        fullName += " (C#)";

        componentID = GetCSharpBindings().requestComponentID(namespaceName, className);
    }

    void CSharpComponent::refresh() {
        componentProperties.clear();
        Scripting::CSClass* clazz = GetCSharpScripting().findClass(namespaceName, className);
        if(!clazz) {
            foundInAssemblies = false;
            return;
        }
        foundInAssemblies = true;

        auto pEntity = GetCSharpBindings().entityToCSObject(getEntity());
        void* args[1] { (MonoObject*)(*pEntity) };
        csComponent = clazz->newObject(args);

        componentProperties = GetCSharpBindings().findAllComponentProperties(namespaceName, className);

        for(auto& property : componentProperties) {
            if(serializedVersion.HasMember(property.serializationName)) {
                switch(property.type) {
                    case Scripting::ComponentType::Int: {
                        std::int32_t v = serializedVersion[property.serializationName].GetInt();
                        property.field->set(*csComponent, Scripting::CSObject((MonoObject*)&v));
                    } break;

                    case Scripting::ComponentType::Float: {
                        float v = serializedVersion[property.serializationName].GetFloat();
                        property.field->set(*csComponent, Scripting::CSObject((MonoObject*)&v));
                    } break;

                    case Scripting::ComponentType::Boolean: {
                        bool v = serializedVersion[property.serializationName].GetBool();
                        property.field->set(*csComponent, Scripting::CSObject((MonoObject*)&v));
                    } break;

                    case Scripting::ComponentType::Entity: {
                        Carrot::UUID uuid = Carrot::UUID::fromString(serializedVersion[property.serializationName].GetString());
                        Entity e = getEntity().getWorld().wrap(uuid);
                        property.field->set(*csComponent, *GetCSharpBindings().entityToCSObject(e));
                    } break;

                    case Scripting::ComponentType::UserDefined:
                    default:
                        Carrot::Log::warn("Property inside %s.%s has type %s which is not supported for serialization", namespaceName.c_str(), className.c_str(), property.typeStr.c_str());
                        break;
                }
            }
        }
    }

    void CSharpComponent::onAssemblyLoad() {
        refresh();
    }

    void CSharpComponent::onAssemblyUnload() {
        serializedVersion = toJSON(serializedDoc);
        csComponent = nullptr;
    }

} // Carrot::ECS