//
// Created by jglrxavpok on 30/05/2023.
//

#include "InspectorPanel.h"

#include <commands/AddComponentsCommand.h>
#include <commands/RemoveComponentsCommand.h>
#include <commands/RenameEntities.h>
#include <core/allocators/InlineAllocator.h>

#include "../Peeler.h"
#include <core/utils/ImGuiUtils.hpp>
#include <engine/ecs/components/CSharpComponent.h>
#include <engine/ecs/EntityTypes.h>
#include <engine/ecs/Prefab.h>
#include <engine/ecs/World.h>
#include <engine/scripting/CSharpBindings.h>
#include <panels/inspector/EditorFunctions.h>

namespace Peeler {
    const char* InspectorPanel::ImGuiWindowID = ICON_FA_WRENCH "  Inspector";

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
            registerComponentEditor(componentID, [](EditContext& edition, const Carrot::Vector<Carrot::ECS::Component*>& components) {
                Carrot::Vector<Carrot::ECS::CSharpComponent*> typedComponents;
                typedComponents.resize(components.size());
                for(std::int64_t i = 0; i < typedComponents.size(); i++) {
                    typedComponents[i] = dynamic_cast<Carrot::ECS::CSharpComponent*>(components[i]);
                }
                editCSharpComponent(edition, typedComponents);
            });
            registerComponentDisplayName(componentID, Carrot::sprintf(ICON_FA_CODE "  %s", componentName.c_str()));
        }
    }

    void InspectorPanel::draw(const Carrot::Render::Context &renderContext) {
        switch(app.inspectorType) {
            case Application::InspectorType::Entities:
                drawEntities(renderContext);
            break;

            case Application::InspectorType::Assets:
                drawAssets(renderContext);
            break;

            case Application::InspectorType::System:
                drawSystem(renderContext);
            break;

            default: TODO;
        }
    }

    void InspectorPanel::focusNameInput() {
        wantsToFocusNameInput = true;
        ImGui::SetWindowFocus(ImGuiWindowID);
    }

    void InspectorPanel::drawEntities(const Carrot::Render::Context& renderContext) {
        if(app.selectedEntityIDs.empty()) {
            return;
        }

        const bool multipleEntities = app.selectedEntityIDs.size() > 1;
        std::unordered_map<Carrot::ComponentID, Carrot::Vector<Carrot::ECS::Component*>> componentsInCommon;
        auto& firstEntityID = app.selectedEntityIDs[0];
        auto& world = app.currentScene.world;
        if(!world.exists(firstEntityID)) {
            ImGui::Text("First selected entity no longer exists?!");
            return;
        }

        // check components the selected entities have in common (multi-edit only works on these components in common)
        auto firstEntityComponents = world.getAllComponents(firstEntityID);
        for(auto& c : firstEntityComponents) {
             componentsInCommon[c->getComponentTypeID()].pushBack(c);
        }

        // remove all components that are not inside in the other entities, and append those who are in common
        for(std::size_t i = 1; i < app.selectedEntityIDs.size(); i++) {
            auto& entityID = app.selectedEntityIDs[i];
            if(!world.exists(entityID)) {
                continue;
            }

            for(auto iter = componentsInCommon.begin(); iter != componentsInCommon.end(); ) {
                auto componentRef = world.getComponent(entityID, iter->first);
                if(componentRef.hasValue()) {
                    iter->second.pushBack(componentRef.asPtr());
                    ++iter;
                } else {
                    iter = componentsInCommon.erase(iter);
                }
            }
        }

        if(componentsInCommon.empty()) {
            ImGui::Text("No components in common between selected entities :(");
        }

        std::unordered_set<std::string> componentsInCommonNames;
        for(const auto& [id, componentList] : componentsInCommon) {
            verify(componentList.size() > 0, "Cannot edit 0 components!");
            componentsInCommonNames.insert(componentList[0]->getName());
        }

        if(multipleEntities) {
            ImGui::Text("%d selected entities", app.selectedEntityIDs.size());
        }

        std::string displayedName = world.getName(firstEntityID);
        if(multipleEntities) {
            for(std::size_t i = 1; i < app.selectedEntityIDs.size(); i++) {
                const std::string& entityName = world.getName(app.selectedEntityIDs[i]);
                if(entityName != displayedName) {
                    displayedName = "<VARIOUS>";
                    break;
                }
            }
        }

        const char* nameFieldID = "Entity name##entity name field inspector";
        if (wantsToFocusNameInput) {
            ImGui::SetKeyboardFocusHere();
            wantsToFocusNameInput = false;
            if (auto* pState = ImGui::GetInputTextState(ImGui::GetID(nameFieldID))) {
                pState->SelectAll();
            }
        }
        if(ImGui::InputText(nameFieldID, displayedName, ImGuiInputTextFlags_EnterReturnsTrue)) {
            // multi-rename
            app.undoStack.push<RenameEntitiesCommand>(app.selectedEntityIDs, displayedName);
        }

        Carrot::Vector<std::string> toRemoveNames;
        Carrot::Vector<Carrot::ComponentID> toRemoveIDs;
        for(auto& [componentID, componentList] : componentsInCommon) {
            EditContext editContext {
                .editor = app,
                .inspector = *this,
                .renderContext = renderContext
            };
            editComponents(editContext, componentID, componentList);
            if(editContext.shouldBeRemoved) {
                toRemoveIDs.pushBack(componentID);
                toRemoveNames.pushBack(componentList[0]->getName());
            }

            if(editContext.hasModifications) {
                app.markDirty();
            }
        }

        if(!toRemoveNames.empty()) {
            app.undoStack.push<RemoveComponentsCommand>(app.selectedEntityIDs, toRemoveNames, toRemoveIDs);
        }

        if(ImGui::Button("Add component##inspector add component")) {
            ImGui::OpenPopup("Add component##Add component popup");
        }

        if(ImGui::BeginPopup("Add component##Add component popup")) {
            const auto& lib = Carrot::ECS::getComponentLibrary();
            for(const auto& compID : lib.getAllIDs()) {
                std::string id = compID;

                if(componentsInCommonNames.contains(compID)) {
                    continue;
                }

                id += "##add component menu item inspector";
                if(ImGui::MenuItem(id.c_str())) {
                    // TODO: find more graceful way to find component ID
                    auto unusedComp = lib.create(compID, app.currentScene.world.wrap(app.selectedEntityIDs[0]));

                    app.undoStack.push<AddComponentsCommand>(app.selectedEntityIDs, Carrot::Vector{compID}, Carrot::Vector{unusedComp->getComponentTypeID()});
                }
            }
            ImGui::EndPopup();
        }
    }

    void InspectorPanel::drawAssets(const Carrot::Render::Context& renderContext) {
        if(app.selectedAssetPaths.empty()) {
            return;
        } else if(app.selectedAssetPaths.size() > 1) {
            for(const auto& path : app.selectedAssetPaths) {
                ImGui::BulletText("%s", path.toString().c_str());
            }
        } else {
            const Carrot::IO::VFS::Path& assetPath = app.selectedAssetPaths[0];
            const std::string assetPathAsStr = assetPath.toString();
            const Carrot::IO::FileFormat fileFormat = Carrot::IO::getFileFormat(assetPathAsStr.c_str());

            ImGui::TextUnformatted(assetPathAsStr.c_str());

            if(Carrot::IO::isImageFormat(fileFormat)) {
                Carrot::Render::Texture::Ref texture = GetAssetServer().blockingLoadTexture(assetPath);
                const float aspectRatio = (float)texture->getSize().width / (float) texture->getSize().height;

                const ImVec2 size = ImVec2(ImGui::GetContentRegionAvailWidth(), ImGui::GetContentRegionAvailWidth() / aspectRatio);
                ImGui::Image(texture->getImguiID(), size);
            }
        }
    }

    void InspectorPanel::drawSystem(const Carrot::Render::Context& renderContext) {
        TODO;
    }

    void InspectorPanel::editComponents(EditContext& editContext, const Carrot::ComponentID& componentID, const Carrot::Vector<Carrot::ECS::Component*>& components) {
        bool shouldKeep = true;
        verify(!components.empty(), "There must be at least one component to edit.");

        std::string displayName;
        auto displayNameIter = displayNames.find(componentID);
        if(displayNameIter == displayNames.end()) {
            displayName = components[0]->getName();
        } else {
            displayName = displayNameIter->second;
        }
        displayName += "##inspector";
        if(ImGui::CollapsingHeader(displayName.c_str(), &shouldKeep)) {
            ImGui::PushID(displayName.c_str());
            auto iter = editionFunctions.find(componentID);
            if(iter == editionFunctions.end()) {
                ImGui::Text("No edition function registered via registerComponentEditor!");
            } else {
                auto& editor = iter->second;
                editor(editContext, components);
            }
            ImGui::PopID();
        }

        if(!shouldKeep) {
            editContext.shouldBeRemoved = true;
        }
    }
} // Peeler