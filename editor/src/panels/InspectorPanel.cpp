//
// Created by jglrxavpok on 30/05/2023.
//

#include "InspectorPanel.h"
#include "../Peeler.h"
#include <core/utils/ImGuiUtils.hpp>
#include <engine/ecs/components/CSharpComponent.h>
#include <engine/ecs/EntityTypes.h>
#include <engine/ecs/World.h>
#include <engine/scripting/CSharpBindings.h>
#include <panels/inspector/EditorFunctions.h>

namespace Peeler {
    InspectorPanel::InspectorPanel(Application& app): EditorPanel(app) {
        registerEditionFunctions(*this);
        registerDisplayNames(*this);
    }

    void InspectorPanel::registerComponentEditor(Carrot::ComponentID componentID, const ComponentEditor& editionFunction) {
        editionFunctions[componentID] = editionFunction;
    }

    void InspectorPanel::registerComponentDisplayName(Carrot::ComponentID componentID, const std::string& name) {
        displayNames[componentID] = name;
    }

    void InspectorPanel::registerCSharpEdition() {
        for(const auto& [componentName, pComponentID] : GetCSharpBindings().getAllComponents()) {
            auto componentID = *pComponentID;
            registerComponentEditor(componentID, [](EditContext& edition, Carrot::ECS::Component* component) {
                editCSharpComponent(edition, dynamic_cast<Carrot::ECS::CSharpComponent*>(component));
            });
            registerComponentDisplayName(componentID, Carrot::sprintf(ICON_FA_CODE "  %s", componentName.c_str()));
        }
    }

    void InspectorPanel::draw(const Carrot::Render::Context &renderContext) {
        if(app.selectedIDs.size() == 1) {
            auto& entityID = app.selectedIDs[0];
            auto& world = app.currentScene.world;
            if(!world.exists(entityID)) {
                ImGui::Text("Entity no longer exists!");
                return;
            }
            auto entity = world.wrap(entityID);
            auto& str = entity.getName();
            ImGui::InputText("Entity name##entity name field inspector", str);

            auto components = world.getAllComponents(entityID);

            std::unordered_set<Carrot::ComponentID> toRemove;
            for(auto& comp : components) {
                EditContext editContext {
                    .editor = app,
                    .inspector = *this,
                    .renderContext = renderContext
                };

                editComponent(editContext, comp);
                if(editContext.shouldBeRemoved) {
                    toRemove.insert(comp->getComponentTypeID());
                }

                if(editContext.hasModifications) {
                    app.markDirty();
                }
            }

            for(const auto& id : toRemove) {
                app.markDirty();
                entity.removeComponent(id);
            }

            if(ImGui::Button("Add component##inspector add component")) {
                ImGui::OpenPopup("Add component##Add component popup");
            }

            if(ImGui::BeginPopup("Add component##Add component popup")) {
                const auto& lib = Carrot::ECS::getComponentLibrary();

                std::unordered_set<std::string> componentsEntityHas;
                for(const auto* comp : entity.getAllComponents()) {
                    componentsEntityHas.insert(comp->getName());
                }
                for(const auto& compID : lib.getAllIDs()) {
                    std::string id = compID;

                    if(componentsEntityHas.contains(id)) {
                        continue;
                    }

                    id += "##add component menu item inspector";
                    if(ImGui::MenuItem(id.c_str())) {
                        auto comp = lib.create(compID, entity);
                        entity.addComponent(std::move(comp));
                        app.markDirty();
                    }
                }
                ImGui::EndPopup();
            }
        } else if(app.selectedIDs.size() > 1) {
            ImGui::Text("Inspector for multiple entities is not supported yet.");
        }
    }

    void InspectorPanel::editComponent(EditContext& editContext, Carrot::ECS::Component* component) {
        bool shouldKeep = true;
        std::string displayName;
        auto displayNameIter = displayNames.find(component->getComponentTypeID());
        if(displayNameIter == displayNames.end()) {
            displayName = component->getName();
        } else {
            displayName = displayNameIter->second;
        }
        displayName += "##" + component->getEntity().getID().toString();
        if(ImGui::CollapsingHeader(displayName.c_str(), &shouldKeep)) {
            ImGui::PushID(displayName.c_str());
            auto iter = editionFunctions.find(component->getComponentTypeID());
            if(iter == editionFunctions.end()) {
                ImGui::Text("No edition function registered via registerComponentEditor!");
            } else {
                auto& editor = iter->second;
                editor(editContext, component);
            }
            ImGui::PopID();
        }

        if(!shouldKeep) {
            editContext.shouldBeRemoved = true;
        }
    }
} // Peeler