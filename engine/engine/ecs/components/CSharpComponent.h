#pragma once

#include <engine/ecs/components/Component.h>
#include <core/scripting/csharp/forward.h>
#include <core/scripting/csharp/CSObject.h>
#include <engine/scripting/CSharpBindings.h>

namespace Carrot::ECS {

    class CSharpComponent : public Carrot::ECS::Component {
    public:
        explicit CSharpComponent(Carrot::ECS::Entity entity, const std::string& namespaceName, const std::string& className);

        explicit CSharpComponent(const Carrot::DocumentElement& doc, Carrot::ECS::Entity entity, const std::string& namespaceName, const std::string& className);

        ~CSharpComponent();

        Carrot::DocumentElement serialise() const override;

        const char *const getName() const override;

        std::unique_ptr <Carrot::ECS::Component> duplicate(const Carrot::ECS::Entity& newOwner) const override;
        static Carrot::DocumentElement serializeProperties(Scripting::CSObject& instance, std::span<const Carrot::Scripting::ReflectionProperty> properties, const std::string& debugName);

        ComponentID getComponentTypeID() const override;

        virtual void repairLinks(const EntityRemappingFunction& remap) override;

    public:
        /**
         * Returns the C# version of this component
         */
        Scripting::CSObject& getCSObject();

        /**
         * Returns true iif the corresponding C# component object was properly loaded.
         * Can return false if there are compilation errors, the assembly was unloaded or the component class no longer
         * exist inside the assembly after a reload
         */
        bool isLoaded();

        /**
         * Properties loaded for this component. Can be modified, but no property can be added/removed here.
         */
        std::span<Scripting::ReflectionProperty> getProperties();

        static void applySavedValues(const Carrot::DocumentElement& savedValues, Scripting::CSObject& instance, std::span<Carrot::Scripting::ReflectionProperty> properties, const Carrot::ECS::World& world, const std::string& debugName);

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
        std::vector<Scripting::ReflectionProperty> componentProperties;
        Scripting::CSharpBindings::Callbacks::Handle loadCallbackHandle;
        Scripting::CSharpBindings::Callbacks::Handle unloadCallbackHandle;

        mutable Carrot::DocumentElement serializedVersion; // always keep the serialized version in case we can't load the component from C#
    };
}