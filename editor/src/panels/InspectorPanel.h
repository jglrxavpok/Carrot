//
// Created by jglrxavpok on 30/05/2023.
//

#pragma once

#include "EditorPanel.h"
#include <core/utils/Identifiable.h>
#include <engine/ecs/components/Component.h>

namespace Peeler {
    struct EditContext {
        Application& editor;
        Carrot::Render::Context renderContext;

        bool shouldBeRemoved = false; // TODO: replace with action stack (for Undo/Redo)
        bool hasModifications = false; // TODO: replace with action stack (for Undo/Redo)
    };

    using ComponentEditor = std::function<void(EditContext& edition, Carrot::ECS::Component* component)>;

    class InspectorPanel: public EditorPanel {
    public:
        explicit InspectorPanel(Application& app);

    public:
        /**
         * Registers a function to draw widgets to edit components which have an ID matching 'componentID'
         * @param componentID
         * @param editionFunction
         */
        void registerComponentEditor(Carrot::ComponentID componentID, const ComponentEditor& editionFunction);

        /**
         * Called when C# module is (re-)loaded, allows inspector to register component editors for C# components
         */
        void registerCSharpEdition();

    public:
        virtual void draw(const Carrot::Render::Context &renderContext) override final;

    private:
        void editComponent(EditContext& editContext, Carrot::ECS::Component* component);

        std::unordered_map<Carrot::ComponentID, ComponentEditor> editionFunctions;
        friend class Application;
    };

} // Peeler
