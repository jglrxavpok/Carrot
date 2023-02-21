//
// Created by jglrxavpok on 16/02/2023.
//

#include "GizmosLayer.h"
#include <engine/Engine.h>
#include <ImGuizmo.h>
#include <engine/render/RenderContext.h>
#include <engine/render/TextureRepository.h>
#include <engine/ecs/World.h>
#include <engine/ecs/components/TransformComponent.h>
#include <glm/gtc/type_ptr.hpp>
#include <core/utils/ImGuiUtils.hpp>
#include "../Peeler.h"

namespace Peeler {
    GizmosLayer::GizmosLayer(Application& editor): ISceneViewLayer(editor),
                                                   translateIcon(GetVulkanDriver(), "resources/textures/ui/translate.png"),
                                                   rotateIcon(GetVulkanDriver(), "resources/textures/ui/rotate.png"),
                                                   scaleIcon(GetVulkanDriver(), "resources/textures/ui/scale.png")
    {}

    bool GizmosLayer::allowCameraMovement() const {
        return !usingGizmo;
    }

    bool GizmosLayer::showLayersBelow() const {
        return false; // probably will always be the bottom one
    }

    void GizmosLayer::draw(const Carrot::Render::Context& renderContext, float startX, float startY) {
        auto& camera = editor.gameViewport.getCamera();
        glm::mat4 identityMatrix = glm::identity<glm::mat4>();
        glm::mat4 cameraView = camera.computeViewMatrix();
        glm::mat4 cameraProjection = camera.getProjectionMatrix();
        cameraProjection[1][1] *= -1;

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(startX, startY, ImGui::GetWindowContentRegionMax().x,
                          ImGui::GetWindowContentRegionMax().y);

        float *cameraViewImGuizmo = glm::value_ptr(cameraView);
        float *cameraProjectionImGuizmo = glm::value_ptr(cameraProjection);

        usingGizmo = false;
        if (editor.selectedIDs.size() == 1) {
            auto transformRef = editor.currentScene.world.getComponent<Carrot::ECS::TransformComponent>(editor.selectedIDs[0]);
            if (transformRef) {
                glm::mat4 transformMatrix = transformRef->toTransformMatrix();

                static ImGuizmo::OPERATION gizmoOperation = ImGuizmo::OPERATION::TRANSLATE;

                ImVec2 selectionWindowPos = ImVec2(startX, startY);

                // some padding
                selectionWindowPos.x += 10.0f;
                selectionWindowPos.y += 10.0f;

                ImGui::SetNextWindowPos(selectionWindowPos, ImGuiCond_Always);
                ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking
                                               | ImGuiWindowFlags_NoTitleBar
                                               | ImGuiWindowFlags_NoCollapse
                                               | ImGuiWindowFlags_NoScrollbar
                                               | ImGuiWindowFlags_AlwaysAutoResize;
                if (ImGui::Begin("##gizmo operation select window", nullptr, windowFlags)) {
                    auto setupSelectedBg = [](bool selected) {
                        if (selected) {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0.5, 0.6));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0.5, 0.8));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0.5, 1.0));
                        } else {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0.3, 0.0));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0.3, 0.3));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0.3, 0.5));
                        }
                    };

                    const float iconSize = 32.0f;

                    setupSelectedBg(gizmoOperation == ImGuizmo::OPERATION::TRANSLATE);
                    if (ImGui::ImageButton(translateIcon.getImguiID(), ImVec2(iconSize, iconSize))) {
                        gizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
                    }
                    ImGui::PopStyleColor(3);

                    ImGui::SameLine();
                    setupSelectedBg(gizmoOperation == ImGuizmo::OPERATION::ROTATE);
                    if (ImGui::ImageButton(rotateIcon.getImguiID(), ImVec2(iconSize, iconSize))) {
                        gizmoOperation = ImGuizmo::OPERATION::ROTATE;
                    }
                    ImGui::PopStyleColor(3);

                    ImGui::SameLine();
                    setupSelectedBg(gizmoOperation == ImGuizmo::OPERATION::SCALE);
                    if (ImGui::ImageButton(scaleIcon.getImguiID(), ImVec2(iconSize, iconSize))) {
                        gizmoOperation = ImGuizmo::OPERATION::SCALE;
                    }
                    ImGui::PopStyleColor(3);
                }
                ImGui::End();

                ImGuizmo::MODE gizmoMode = ImGuizmo::MODE::LOCAL;

                bool used = ImGuizmo::Manipulate(
                        cameraViewImGuizmo,
                        cameraProjectionImGuizmo,
                        gizmoOperation,
                        gizmoMode,
                        glm::value_ptr(transformMatrix)
                );

                if (used) {
                    glm::mat4 parentMatrix = glm::identity<glm::mat4>();
                    auto parentEntity = transformRef->getEntity().getParent();
                    if (parentEntity) {
                        auto parentTransform = parentEntity->getComponent<Carrot::ECS::TransformComponent>();
                        if (parentTransform) {
                            parentMatrix = parentTransform->toTransformMatrix();
                        }
                    }

                    glm::mat4 localTransform = glm::inverse(parentMatrix) * transformMatrix;
                    float translation[3] = {0.0f};
                    float scale[3] = {0.0f};
                    float rotation[3] = {0.0f};
                    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localTransform), translation, rotation,
                                                          scale);

                    transformRef->localTransform.position = glm::vec3(translation[0], translation[1],
                                                                      translation[2]);
                    transformRef->localTransform.rotation = glm::quat(
                            glm::radians(glm::vec3(rotation[0], rotation[1], rotation[2])));
                    transformRef->localTransform.scale = glm::vec3(scale[0], scale[1], scale[2]);

                    editor.markDirty();
                }

                usingGizmo = ImGuizmo::IsUsing() || ImGuizmo::IsOver();
            }
        }
    }

    bool GizmosLayer::allowSceneEntityPicking() const {
        return !usingGizmo;
    }
} // Peeler