#pragma once

#include <engine/ecs/components/Component.h>
#include <core/scripting/csharp/forward.h>
#include <core/scripting/csharp/CSObject.h>
#include <engine/scripting/CSharpBindings.h>

namespace Carrot::ECS {

    class CSharpComponent : public Carrot::ECS::Component {
    public:
        explicit CSharpComponent(Carrot::ECS::Entity entity, const std::string& namespaceName, const std::string& className);

        explicit CSharpComponent(const rapidjson::Value& json, Carrot::ECS::Entity entity, const std::string& namespaceName, const std::string& className);

        ~CSharpComponent();

        rapidjson::Value toJSON(rapidjson::Document& doc) const override;

        const char *const getName() const override;

        std::unique_ptr <Carrot::ECS::Component> duplicate(const Carrot::ECS::Entity& newOwner) const override;

        ComponentID getComponentTypeID() const override;

        virtual void repairLinks(const EntityRemappingFunction& remap) override;

    public:
        /**
         * Returns the C# sharp of this component
         */
        Scripting::CSObject& getCSComponentObject();

        /**
         * Returns true iif the corresponding C# component object was properly loaded.
         * Can return false if there are compilation errors, the assembly was unloaded or the component class no longer
         * exist inside the assembly after a reload
         */
        bool isLoaded();

        /**
         * Properties loaded for this component. Can be modified, but no property can be added/removed here.
         */
        std::span<Scripting::ComponentProperty> getProperties();

    private:
        void init();

        void refresh();
        void onAssemblyLoad();
        void onAssemblyUnload();

    private:
        bool foundInAssemblies = false;
        std::string namespaceName;
        std::string className;
        std::string fullName;

        Carrot::ComponentID componentID = -1;
        std::shared_ptr<Scripting::CSObject> csComponent;
        std::vector<Scripting::ComponentProperty> componentProperties;
        Scripting::CSharpBindings::Callbacks::Handle loadCallbackHandle;
        Scripting::CSharpBindings::Callbacks::Handle unloadCallbackHandle;

        mutable rapidjson::Document serializedDoc{rapidjson::kObjectType};
        mutable rapidjson::Value serializedVersion{rapidjson::kObjectType}; // always keep the serialized version in case we can't load the component from C#
    };
}