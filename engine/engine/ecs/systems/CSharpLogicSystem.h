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

        explicit CSharpLogicSystem(const rapidjson::Value& json, Carrot::ECS::World& world);

        ~CSharpLogicSystem();

        virtual void tick(double dt) override;

        virtual void onFrame(Carrot::Render::Context renderContext) override;

        const char *getName() const override;

    public:
        virtual void onEntitiesAdded(const std::vector<EntityID>& entities) override;
        virtual void onEntitiesRemoved(const std::vector<EntityID>& entities) override;
        virtual void onEntitiesUpdated(const std::vector<EntityID>& entities) override;

    public:
        virtual std::unique_ptr<Carrot::ECS::System> duplicate(Carrot::ECS::World& newOwner) const override;

        rapidjson::Value toJSON(rapidjson::Document::AllocatorType& allocator) const override;

    public:
        Scripting::CSArray* getEntityList();

    //private:
        void init();

        void recreateEntityList();

        void onAssemblyLoad();
        void onAssemblyUnload();

    private:
        std::shared_ptr<Scripting::CSObject> csSystem;
        Scripting::CSMethod* csTickMethod = nullptr;
        std::shared_ptr<Scripting::CSArray> csEntities = nullptr;

        std::string namespaceName;
        std::string className;
        std::string systemName;
        bool foundInAssemblies = false; // are the given namespace+class name inside a loaded assembly?

        Scripting::CSharpBindings::Callbacks::Handle loadCallbackHandle;
        Scripting::CSharpBindings::Callbacks::Handle unloadCallbackHandle;
    };

} // Carrot::ECS