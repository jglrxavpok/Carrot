//
// Created by jglrxavpok on 30/05/2023.
//

#pragma once

#include <core/containers/Vector.hpp>

#include "EditorPanel.h"
#include <core/utils/Identifiable.h>
#include <engine/render/resources/Texture.h>
#include <engine/ecs/components/Component.h>

namespace Carrot {
    class Pipeline;
}

namespace Peeler {
    class InspectorPanel;

    struct EditContext {
        Application& editor;
        InspectorPanel& inspector;
        Carrot::Render::Context renderContext;

        bool shouldBeRemoved = false; // TODO: replace with action stack (for Undo/Redo)
        bool hasModifications = false; // TODO: replace with action stack (for Undo/Redo)
    };

    using ComponentEditor = std::function<void(EditContext& edition, const Carrot::Vector<Carrot::ECS::Component*>& components)>;

    class InspectorPanel: public EditorPanel {
    public:
        explicit InspectorPanel(Application& app);

    public:
        /**
         * Registers a function to draw widgets to edit components which have an ID matching 'componentID'
         */
        void registerComponentEditor(Carrot::ComponentID componentID, const ComponentEditor& editionFunction);

        /**
         * Registers the name to display for a given component. If none is registered, will display the component's name (Component::getName)
         */
        void registerComponentDisplayName(Carrot::ComponentID componentID, const std::string& displayName);

        /**
         * Called when C# module is (re-)loaded, allows inspector to register component editors for C# components
         */
        void registerCSharpEdition();

    public:
        virtual void draw(const Carrot::Render::Context &renderContext) override final;

    public: // widgets
        /**
         * Returns true iif an entity was picked this frame. Stores the result in 'destination' if one was found
         */
        bool drawPickEntityWidget(const char* label, Carrot::ECS::Entity* destination);

        /**
         * Returns true iif the texture selection was changed
         */
        bool drawPickTextureWidget(const char* label, Carrot::Render::Texture::Ref* pDestination, bool allowNull = true);

        /**
         * Returns true iif the pipeline selection was changed
         */
        bool drawPickPipelineWidget(const char* label, std::shared_ptr<Carrot::Pipeline>* pDestination, bool allowNull = true);

    private:
        // used when entities are selected
        void drawEntities(const Carrot::Render::Context& renderContext);

        // used when assets are selected
        void drawAssets(const Carrot::Render::Context& renderContext);

        // used when systems are selected
        void drawSystem(const Carrot::Render::Context& renderContext);

        void drawPrefab(const Carrot::Render::Context& renderContext, const Carrot::IO::VFS::Path& vfsPath);

        void editComponents(EditContext& editContext, const Carrot::ComponentID& componentID, const Carrot::Vector<Carrot::ECS::Component*>& components);

        std::unordered_map<Carrot::ComponentID, ComponentEditor> editionFunctions;
        std::unordered_map<Carrot::ComponentID, std::string> displayNames;
        friend class Application;
    };

} // Peeler
