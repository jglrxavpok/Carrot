//
// Created by jglrxavpok on 25/02/2025.
//

#include <Peeler.h>
#include <commands/ModifySystemsCommands.h>
#include <engine/ecs/Prefab.h>
#include <engine/ecs/components/PrefabInstanceComponent.h>

namespace Peeler {
    void Application::drawEntityErrors(const ErrorReport& report, Carrot::ECS::Entity entity, const char* uniqueWidgetID) {
        auto iter = report.errorMessagesPerEntity.find(entity.getID());
        if (iter == report.errorMessagesPerEntity.end()) {
            return;
        }
        ImGui::PushID(uniqueWidgetID);
        CLEANUP(ImGui::PopID());

        const char* icon = ICON_FA_CIRCLE_EXCLAMATION;
        const char* popupID = "Errors";

        const Carrot::Vector<std::string>& errors = iter->second;
        ImGui::PushStyleColor(ImGuiCol_Text, 0xFF0000FF);
        if(ImGui::SmallButton(icon)) {
            ImGui::OpenPopup(popupID);
        }
        ImGui::PopStyleColor(1);

        if (ImGui::BeginPopup(popupID)) {
            for (i32 errorIndex = 0; errorIndex < errors.size(); ++errorIndex) {
                if (errorIndex != 0) {
                    ImGui::Separator();
                }

                ImGui::TextUnformatted(errors[errorIndex].c_str());
            }
            ImGui::EndPopup();
        }
    }

    void Application::drawEntityWarnings(Carrot::ECS::Entity entity, const char* uniqueWidgetID) {
        if (isCurrentlyPlaying()) {
            return; // keep fps for game
        }
        struct Warning {
            std::string message;
            std::function<void()> quickFix;
        };

        // static to reuse across entities and frames
        static Carrot::Vector<std::string> missingLogicSystems;
        static Carrot::Vector<std::string> missingRenderSystems;

        static std::unordered_set<std::string> sceneLogicSystems;
        static std::unordered_set<std::string> sceneRenderSystems;
        static std::unordered_set<std::string> prefabLogicSystems;
        static std::unordered_set<std::string> prefabRenderSystems;

        missingLogicSystems.clear();
        missingRenderSystems.clear();
        sceneLogicSystems.clear();
        sceneRenderSystems.clear();
        prefabLogicSystems.clear();
        prefabRenderSystems.clear();

        ImGui::PushID(uniqueWidgetID);
        CLEANUP(ImGui::PopID());

        //Carrot::Vector<Warning> warnings;
        std::vector<Warning> warnings; // wtf why does std::vector work but not mine?
        auto& systemsLib = Carrot::ECS::getSystemLibrary();

        // check if current scene has all systems of the prefab
        if (const auto pPrefabComponent = entity.getComponent<Carrot::ECS::PrefabInstanceComponent>()) {
            auto pPrefab = pPrefabComponent->prefab;
            if (pPrefabComponent->childID == Carrot::ECS::Prefab::PrefabRootUUID) {
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
                    warnings.emplace_back(message, [&systemsLib, this]() {
                        undoStack.push<AddSystemsCommand>(missingLogicSystems, false);
                    });
                }
                if (!missingRenderSystems.empty()) {
                    std::string message = "Current scene is missing render systems used inside this prefab.\nThis prefab may not work properly.\nMissing systems:\n";
                    for (const auto& systemName : missingRenderSystems) {
                        message += "- ";
                        message += systemName;
                        message += '\n';
                    }
                    warnings.emplace_back(message, [&systemsLib, this]() {
                        undoStack.push<AddSystemsCommand>(missingRenderSystems, true);
                    });
                }
            }
        }

        if (warnings.empty()) {
            return;
        }
        const char* icon = ICON_FA_TRIANGLE_EXCLAMATION;
        const char* popupID = "Warnings";

        ImGui::PushStyleColor(ImGuiCol_Text, 0xFF00FFFF);
        if(ImGui::SmallButton(icon)) {
            ImGui::OpenPopup(popupID);
        }
        ImGui::PopStyleColor(1);

        if (ImGui::BeginPopup(popupID)) {
            for (i32 warningIndex = 0; warningIndex < warnings.size(); ++warningIndex) {
                if (warningIndex != 0) {
                    ImGui::Separator();
                }

                Warning& warning = warnings[warningIndex];
                ImGui::TextUnformatted(warning.message.c_str());
                if (warning.quickFix) {
                    if(ImGui::Button("Quick fix")) {
                        (warning.quickFix)();
                    }
                }
            }
            ImGui::EndPopup();
        }
    }
}
