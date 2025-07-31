#include "CSharpLogicSystem.h"
#include "mono/metadata/object.h"
#include "core/io/Logging.hpp"
#include <engine/Engine.h>
#include <core/scripting/csharp/Engine.h>
#include <core/scripting/csharp/CSArray.h>
#include <core/scripting/csharp/CSObject.h>
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/CSMethod.h>
#include <engine/ecs/components/CSharpComponent.h>
#include <engine/physics/PhysicsSystem.h>

namespace Carrot::ECS {
    CSharpLogicSystem::CSharpLogicSystem(Carrot::ECS::World& world, const std::string& namespaceName, const std::string& className) : Carrot::ECS::System(world),
        namespaceName(namespaceName), className(className)
    {
        loadCallbackHandle = GetCSharpBindings().registerGameAssemblyLoadCallback([&]() { onAssemblyLoad(); });
        unloadCallbackHandle = GetCSharpBindings().registerGameAssemblyUnloadCallback([&]() { onAssemblyUnload(); });

        init(false);
    }

    CSharpLogicSystem::CSharpLogicSystem(const Carrot::DocumentElement& doc, Carrot::ECS::World& world, const std::string& namespaceName, const std::string& className):
        Carrot::ECS::System(doc, world), namespaceName(namespaceName), className(className) {
        loadCallbackHandle = GetCSharpBindings().registerGameAssemblyLoadCallback([&]() { onAssemblyLoad(); });
        unloadCallbackHandle = GetCSharpBindings().registerGameAssemblyUnloadCallback([&]() { onAssemblyUnload(); });

        // TODO: load from JSON
        init(false);
    }

    CSharpLogicSystem::~CSharpLogicSystem() {
        GetCSharpBindings().unregisterGameAssemblyLoadCallback(loadCallbackHandle);
        GetCSharpBindings().unregisterGameAssemblyUnloadCallback(unloadCallbackHandle);
    }

    void CSharpLogicSystem::init(bool needToReloadEntities) {
        systemName = namespaceName;
        systemName += '.';
        systemName += className;
        systemName += " (C#)";

        Scripting::CSClass* clazz = GetCSharpScripting().findClass(namespaceName, className);
        if(!clazz) {
            foundInAssemblies = false;
            return;
        }
        foundInAssemblies = true;
        for (Scripting::CSClass* c = clazz; c != nullptr; c = GetCSharpScripting().getParentClass(*c)) {
            if (csTickMethod == nullptr) {
                csTickMethod = c->findMethod("Tick", 1);
            }
            if (csPrePhysicsTickMethod == nullptr) {
                csPrePhysicsTickMethod = c->findMethod("PrePhysicsTick", 1);
            }
            if (csPostPhysicsTickMethod == nullptr) {
                csPostPhysicsTickMethod = c->findMethod("PostPhysicsTick", 1);
            }
        }
        std::uint64_t ptr = reinterpret_cast<std::uint64_t>(this);
        void* args[1] { (void*)&ptr };
        csSystem = clazz->newObject(args);

        Scripting::CSClass* signatureClass = GetCSharpScripting().findClass("Carrot", "Signature");
        verify(signatureClass, "Could not find Carrot.Signature class");

        Scripting::CSClass* systemClass = GetCSharpScripting().findClass("Carrot", "System");
        verify(systemClass, "Could not find Carrot.System class");
        Scripting::CSField* signatureField = systemClass->findField("_signature");
        verify(signatureField, "Could not find Carrot.System::_signature field");

        Scripting::CSObject signatureObj = signatureField->get(*csSystem);

        Scripting::CSClass* bitArrayClass = GetCSharpScripting().findClass("System.Collections", "BitArray");
        verify(bitArrayClass, "Could not find System.Collections.BitArray class")

        Scripting::CSField* componentsField = signatureClass->findField("_components");
        verify(componentsField, "Could not find Carrot.Signature::_components field");

        Scripting::CSObject bitArrayObj = componentsField->get(signatureObj);

        Scripting::CSMethod* getMethod = bitArrayClass->findMethod("Get");
        verify(bitArrayClass, "Could not find System.Collections.BitArray::Get method")

        signature.clear();
        for (std::size_t j = 0; j < MAX_COMPONENTS; ++j) {
            void* args[1] = {
                    &j
            };
            bool hasComponent = *((bool*)mono_object_unbox(getMethod->invoke(bitArrayObj, args)));
            if(hasComponent) {
                signature.addComponentFromComponentIndex_Internal(j);
            }
        }

        if(needToReloadEntities) {
            world.reloadSystemEntities(this);
            recreateEntityList();
        }
    }

    void CSharpLogicSystem::tick(double dt) {
        ZoneScopedN("C# tick");
        ZoneName(systemName.c_str(), systemName.size());
        if(!foundInAssemblies) {
            return;
        }
        if(*csSystem) {
            void* args[1] { (void*)&dt };
            csTickMethod->invoke(*csSystem, args);
        }
    }

    void CSharpLogicSystem::prePhysics() {
        ZoneScopedN("C# pre physics");
        ZoneName(systemName.c_str(), systemName.size());
        if(!foundInAssemblies) {
            return;
        }
        if(*csSystem && csPrePhysicsTickMethod) {
            double dt = Physics::PhysicsSystem::TimeStep;
            void* args[1] { (void*)&dt };
            csPrePhysicsTickMethod->invoke(*csSystem, args);
        }
    }

    void CSharpLogicSystem::postPhysics() {
        ZoneScopedN("C# post physics");
        ZoneName(systemName.c_str(), systemName.size());
        if(!foundInAssemblies) {
            return;
        }
        if(*csSystem && csPostPhysicsTickMethod) {
            double dt = Physics::PhysicsSystem::TimeStep;
            void* args[1] { (void*)&dt };
            csPostPhysicsTickMethod->invoke(*csSystem, args);
        }
    }

    void CSharpLogicSystem::onFrame(Carrot::Render::Context renderContext) {
        // TODO: autogenerated
    }

    void CSharpLogicSystem::onEntitiesAdded(const std::vector<EntityID>& entities) {
        System::onEntitiesAdded(entities);
        recreateEntityList();
    }

    void CSharpLogicSystem::onEntitiesRemoved(const std::vector<EntityID>& entities) {
        System::onEntitiesRemoved(entities);
        recreateEntityList();
    }

    void CSharpLogicSystem::onEntitiesUpdated(const std::vector<EntityID>& entities) {
        System::onEntitiesUpdated(entities);
        recreateEntityList();
    }

    void CSharpLogicSystem::recreateEntityList() {
        if(!csSystem) {
            return;
        }

        csEntities = GetCSharpBindings().entityListToCSharp(entitiesWithComponents);
    }

    std::unique_ptr <Carrot::ECS::System> CSharpLogicSystem::duplicate(Carrot::ECS::World& newWorld) const {
        std::unique_ptr<CSharpLogicSystem> result = std::make_unique<CSharpLogicSystem>(newWorld, namespaceName, className);
        return result;
    }

    const char *CSharpLogicSystem::getName() const {
        return systemName.c_str();
    }

    Carrot::DocumentElement CSharpLogicSystem::serialise() const {
        // TODO
        return System::serialise();
    }

    Scripting::CSArray* CSharpLogicSystem::getEntityList() {
        return csEntities.get();
    }

    void CSharpLogicSystem::onAssemblyLoad() {
        init(true);
    }

    void CSharpLogicSystem::onAssemblyUnload() {
        csSystem = nullptr;
        csTickMethod = nullptr;
        csPrePhysicsTickMethod = nullptr;
        csPostPhysicsTickMethod = nullptr;
        csEntities = nullptr;
    }
}
