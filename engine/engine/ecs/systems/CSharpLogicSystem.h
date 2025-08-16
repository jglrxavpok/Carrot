#pragma once

#include <cstdint>
#include <functional>
#include <engine/ecs/systems/System.h>
#include <core/utils/Lookup.hpp>
#include <core/scripting/csharp/CSObject.h>
#include <engine/scripting/CSharpBindings.h>

namespace Carrot::ECS {
    class CSharpLogicSystem : public Carrot::ECS::System {
    public:
        explicit CSharpLogicSystem(Carrot::ECS::World& world, const std::string& namespaceName, const std::string& className);

        explicit CSharpLogicSystem(const Carrot::DocumentElement& doc, Carrot::ECS::World& world, const std::string& namespaceName, const std::string& className);

        ~CSharpLogicSystem();

        virtual void firstTick() override;
        virtual void tick(double dt) override;
        virtual void prePhysics() override;
        virtual void postPhysics() override;

        virtual void onFrame(Carrot::Render::Context renderContext) override;

        const char *getName() const override;

    public:
        virtual void onEntitiesAdded(const std::vector<EntityID>& entities) override;
        virtual void onEntitiesRemoved(const std::vector<EntityID>& entities) override;
        virtual void onEntitiesUpdated(const std::vector<EntityID>& entities) override;

        void reload() override;

        virtual std::unique_ptr<Carrot::ECS::System> duplicate(Carrot::ECS::World& newOwner) const override;

        Carrot::DocumentElement serialise() const override;

    public:
        Scripting::CSArray* getEntityList();

    private:
        /**
         * Loads the system from C# assemblies (engine + game)
         * @param needToReloadEntities whether to ask the world to reload entities for this system.
         *  Set to false in constructors because world copy would re-add entities afterwards (so entities would be multiple times inside the 'entities' vector)
         */
        void init(bool needToReloadEntities);

        void recreateEntityList();

        void onAssemblyLoad();
        void onAssemblyUnload();

    private:
        std::shared_ptr<Scripting::CSObject> csSystem;
        Scripting::CSMethod* csTickMethod = nullptr;
        Scripting::CSMethod* csFirstTickMethod = nullptr;
        Scripting::CSMethod* csFirstTickOfEntitiesMethod = nullptr;
        Scripting::CSMethod* csPrePhysicsTickMethod = nullptr;
        Scripting::CSMethod* csPostPhysicsTickMethod = nullptr;
        std::shared_ptr<Scripting::CSArray> csEntities = nullptr; // array of Carrot::System.EntityWithComponents

        std::string namespaceName;
        std::string className;
        std::string systemName;
        bool foundInAssemblies = false; // are the given namespace+class name inside a loaded assembly?

        Scripting::CSharpBindings::Callbacks::Handle loadCallbackHandle;
        Scripting::CSharpBindings::Callbacks::Handle unloadCallbackHandle;
    };

} // Carrot::ECS