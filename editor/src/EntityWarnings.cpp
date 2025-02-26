//
// Created by jglrxavpok on 25/02/2025.
//

#include <Peeler.h>
#include <commands/ModifySystemsCommands.h>
#include <engine/ecs/Prefab.h>
#include <engine/ecs/components/PrefabInstanceComponent.h>

namespace Peeler {
    void Application::drawEntityWarnings(Carrot::ECS::Entity entity, const char* uniqueWidgetID) {
        struct Warning {
            std::string message;
            std::optional<std::function<void()>> quickFix;
        };

        ImGui::PushID(uniqueWidgetID);
        CLEANUP(ImGui::PopID());

        Carrot::Vector<Warning> warnings;
        auto& systemsLib = Carrot::ECS::getSystemLibrary();

        // check if current scene has all systems of the prefab
        if (const auto pPrefabComponent = entity.getComponent<Carrot::ECS::PrefabInstanceComponent>()) {
            auto pPrefab = pPrefabComponent->prefab;
            if (pPrefabComponent->childID == Carrot::ECS::Prefab::PrefabRootUUID) {
                Carrot::Vector<std::string> missingLogicSystems;
                Carrot::Vector<std::string> missingRenderSystems;

                std::unordered_set<std::string> sceneLogicSystems;
                std::unordered_set<std::string> sceneRenderSystems;
                std::unordered_set<std::string> prefabLogicSystems;
                std::unordered_set<std::string> prefabRenderSystems;
                for (const Carrot::ECS::System* pSystem : currentScene.world.getLogicSystems()) {
                    sceneLogicSystems.insert(pSystem->getName());
                }
                for (const Carrot::ECS::System* pSystem : currentScene.world.getRenderSystems()) {
                    sceneRenderSystems.insert(pSystem->getName());
                }
                for (const Carrot::ECS::System* pSystem : pPrefab->getBaseSceneLogicSystems()) {
                    prefabLogicSystems.insert(pSystem->getName());
                }
                for (const Carrot::ECS::System* pSystem : pPrefab->getBaseSceneRenderSystems()) {
                    prefabRenderSystems.insert(pSystem->getName());
                }

                for (const std::string& inPrefab : prefabLogicSystems) {
                    if (!sceneLogicSystems.contains(inPrefab)) {
                        missingLogicSystems.emplaceBack(inPrefab);
                    }
                }
                for (const std::string& inPrefab : prefabRenderSystems) {
                    if (!sceneRenderSystems.contains(inPrefab)) {
                        missingRenderSystems.emplaceBack(inPrefab);
                    }
                }

                if (!missingLogicSystems.empty()) {
                    std::string message = "Current scene is missing logic systems used inside this prefab.\nThis prefab may not work properly.\nMissing systems:\n";
                    for (const auto& systemName : missingLogicSystems) {
                        message += "- ";
                        message += systemName;
                        message += '\n';
                    }
                    warnings.emplaceBack(message, [&systemsLib, this, missingLogicSystems]() {
                        for (const auto& systemName : missingLogicSystems) {
                            undoStack.push<AddSystemCommand>(systemName, false);
                        }
                    });
                }
                if (!missingRenderSystems.empty()) {
                    std::string message = "Current scene is missing render systems used inside this prefab.\nThis prefab may not work properly.\nMissing systems:\n";
                    for (const auto& systemName : missingRenderSystems) {
                        message += "- ";
                        message += systemName;
                        message += '\n';
                    }
                    warnings.emplaceBack(message, [&systemsLib, this, missingRenderSystems]() {
                        for (const auto& systemName : missingRenderSystems) {
                            undoStack.push<AddSystemCommand>(systemName, true);
                        }
                    });
                }
            }
        }

        if (warnings.empty()) {
            return;
        }
        const char* icon = ICON_FA_EXCLAMATION_TRIANGLE;
        const char* popupID = "Warnings";
        if(ImGui::SmallButton(icon)) {
            ImGui::OpenPopup(popupID);
        }

        if (ImGui::BeginPopup(popupID)) {
            for (i32 warningIndex = 0; warningIndex < warnings.size(); ++warningIndex) {
                if (warningIndex != 0) {
                    ImGui::Separator();
                }

                Warning& warning = warnings[warningIndex];
                ImGui::TextUnformatted(warning.message.c_str());
                if (warning.quickFix.has_value()) {
                    if(ImGui::Button("Quick fix")) {
                        (warning.quickFix.value())();
                    }
                }
            }
            ImGui::EndPopup();
        }
    }
}
