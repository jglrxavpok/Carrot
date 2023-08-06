//
// Created by jglrxavpok on 02/08/2023.
//

#include "NavMeshPanel.h"
#include <core/utils/ImGuiUtils.hpp>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/ModelComponent.h>
#include "../Peeler.h"

namespace Peeler {
    NavMeshPanel::NavMeshPanel(Peeler::Application& app): EditorPanel(app) {

    }

    void NavMeshPanel::draw(const Carrot::Render::Context& renderContext) {
        ImGui::Text("%llu selected entities", app.selectedIDs.size());

        auto& world = app.currentScene.world;
        bool waitForBaking = false;
        for(const auto& entityID : app.selectedIDs) {
            Carrot::ECS::Entity entity = world.wrap(entityID);

            if(auto transformCompRef = entity.getComponent<Carrot::ECS::TransformComponent>()) {
                if(auto modelRef = entity.getComponent<Carrot::ECS::ModelComponent>()) {
                    ImGui::Text("Valid entity: %s", entity.getName().c_str());
                    waitForBaking |= !modelRef->asyncModel.isReady() && !modelRef->asyncModel.isEmpty();
                }
            }
        }

        // TODO: schema for explanation (like Unity does it)
        //  + help markers
        ImGui::DragFloat("Minimum width", &minimumWidth, 0.1f, 0.0001f, FLT_MAX);
        ImGui::DragFloat("Minimum clearance", &minimumClearance, 0.1f, 0.0001f, FLT_MAX);
        ImGui::DragFloat("Voxel size", &voxelSize, 0.01f, 0.01f, 10.0f);
        ImGui::SliderAngle("Maximum slope angle", &maximumSlope, 0.0f, 90.0f, "%.0f°");

        const bool disabled = waitForBaking || navMeshBuilder.isRunning();
        ImGui::BeginDisabled(disabled);
        if(ImGui::Button("Bake")) {
            // avoid memory cost of copy each frame
            std::vector<Carrot::AI::NavMeshBuilder::MeshEntry> buildEntries;
            for(const auto& entityID : app.selectedIDs) {
                Carrot::ECS::Entity entity = world.wrap(entityID);

                if(auto transformCompRef = entity.getComponent<Carrot::ECS::TransformComponent>()) {
                    if(auto modelRef = entity.getComponent<Carrot::ECS::ModelComponent>()) {
                        buildEntries.emplace_back(Carrot::AI::NavMeshBuilder::MeshEntry {
                            .model = modelRef->asyncModel.get(),
                            .transform = transformCompRef->toTransformMatrix()
                        });
                    }
                }
            }

            Carrot::AI::NavMeshBuilder::BuildParams params {
                .voxelSize = voxelSize,
                .maxSlope = maximumSlope,
                .characterHeight = minimumClearance,
                .characterRadius = minimumWidth,
            };
            navMeshBuilder.start(std::move(buildEntries), params);
        }
        ImGui::EndDisabled();

        ImGui::TextUnformatted(navMeshBuilder.getDebugStep().c_str());

        if(!navMeshBuilder.isRunning()) {
            navMeshBuilder.debugDraw(GetEngine().newRenderContext(renderContext.swapchainIndex, app.gameViewport));
        }
    }
} // Peeler