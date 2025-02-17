#include "CSharpComponent.h"
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/Engine.h>
#include <core/async/ParallelMap.hpp>
#include <engine/utils/Macros.h>
#include <engine/Engine.h>
#include <core/io/Logging.hpp>

namespace Carrot::ECS {

    CSharpComponent::CSharpComponent(Carrot::ECS::Entity entity, const std::string& namespaceName, const std::string& className) : Carrot::ECS::Component(
            std::move(entity)), namespaceName(namespaceName), className(className) {
        loadCallbackHandle = GetCSharpBindings().registerGameAssemblyLoadCallback([&]() { onAssemblyLoad(); });
        unloadCallbackHandle = GetCSharpBindings().registerGameAssemblyUnloadCallback([&]() { onAssemblyUnload(); });

        init();
        refresh();
    }

    CSharpComponent::CSharpComponent(const Carrot::DocumentElement& doc, Carrot::ECS::Entity entity, const std::string& namespaceName, const std::string& className) : Carrot::ECS::Component(
            std::move(entity)), namespaceName(namespaceName), className(className) {
        serializedVersion = doc;
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
        serializedVersion = serialise();
        auto result = std::make_unique<CSharpComponent>(serializedVersion, newOwner, namespaceName, className);
        return result;
    }

    Carrot::DocumentElement CSharpComponent::serialise() const {
        if(!foundInAssemblies) { // copy last known state
            return serializedVersion;
        }

        Carrot::DocumentElement r;

        for(auto& property : componentProperties) {
            const std::string& key = property.serializationName;
            switch(property.type) {
                case Scripting::ComponentType::Int: {
                    auto v = property.field->get(*csComponent).unbox<std::int32_t>();
                    r[key] = i64{v};
                } break;

                case Scripting::ComponentType::Float: {
                    auto v = property.field->get(*csComponent).unbox<float>();
                    r[key] = v;
                } break;

                case Scripting::ComponentType::Boolean: {
                    auto v = property.field->get(*csComponent).unbox<bool>();
                    r[key] = v;
                } break;

                case Scripting::ComponentType::Entity: {
                    MonoObject* entityObj = property.field->get(*csComponent);
                    ECS::Entity entity = GetCSharpBindings().convertToEntity(entityObj);
                    r[key] = entity.getID().toString();
                } break;

                case Scripting::ComponentType::String: {
                    MonoString* text = reinterpret_cast<MonoString *>(property.field->get(*csComponent).toMono());
                    char* textReadable = mono_string_to_utf8(text);
                    CLEANUP(mono_free(textReadable));
                    r[key] = textReadable;
                } break;

                case Scripting::ComponentType::UserEnum: {
                    const std::string enumValueAsString = GetCSharpBindings().enumValueToString(property.field->get(*csComponent));
                    r[key] = enumValueAsString;
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
            if(serializedVersion.contains(property.serializationName)) {
                switch(property.type) {
                    case Scripting::ComponentType::Int: {
                        i32 v = static_cast<i32>(serializedVersion[property.serializationName].getAsInt64());
                        property.field->set(*csComponent, Scripting::CSObject((MonoObject*)&v));
                    } break;

                    case Scripting::ComponentType::Float: {
                        float v = static_cast<float>(serializedVersion[property.serializationName].getAsDouble());
                        property.field->set(*csComponent, Scripting::CSObject((MonoObject*)&v));
                    } break;

                    case Scripting::ComponentType::Boolean: {
                        bool v = serializedVersion[property.serializationName].getAsBool();
                        property.field->set(*csComponent, Scripting::CSObject((MonoObject*)&v));
                    } break;

                    case Scripting::ComponentType::Entity: {
                        Carrot::UUID uuid = Carrot::UUID::fromString(serializedVersion[property.serializationName].getAsString());
                        Entity e = getEntity().getWorld().wrap(uuid);
                        property.field->set(*csComponent, *GetCSharpBindings().entityToCSObject(e));
                    } break;

                    case Scripting::ComponentType::UserEnum: {
                        Scripting::CSObject enumValue = GetCSharpBindings().stringToEnumValue(property.typeStr, std::string { serializedVersion[property.serializationName].getAsString() });
                        property.field->set(*csComponent, enumValue);
                    } break;

                    case Scripting::ComponentType::String: {
                        property.field->set(*csComponent, Scripting::CSObject(reinterpret_cast<MonoObject*>(mono_string_new_wrapper(std::string{serializedVersion[property.serializationName].getAsString()}.c_str()))));
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
        serializedVersion = serialise();
        csComponent = nullptr;
    }

} // Carrot::ECS