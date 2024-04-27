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

        static ImGuizmo::OPERATION gizmoOperation = ImGuizmo::OPERATION::TRANSLATE;

        // setup window with the buttons to switch gizmos
        ImVec2 selectionWindowPos = ImVec2(startX, startY);
        // some padding
        selectionWindowPos.x += 10.0f;
        selectionWindowPos.y += 10.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0.0, 0.0));
        CLEANUP(ImGui::PopStyleColor());
        CLEANUP(ImGui::PopStyleVar());

        ImGui::SetNextWindowPos(selectionWindowPos, ImGuiCond_Always);
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking
                                       | ImGuiWindowFlags_NoTitleBar
                                       | ImGuiWindowFlags_NoCollapse
                                       | ImGuiWindowFlags_NoScrollbar
                                       | ImGuiWindowFlags_AlwaysAutoResize;

        if (ImGui::Begin("##gizmo operation select window", nullptr, windowFlags)) {
            auto setupSelectedBg = [](bool selected) {
                if (selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(2.0f / 255.0f, 2.0f / 255.0f, 110.0f / 255.0f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f / 255.0f, 1.0f / 255.0f, 123.0f / 255.0f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0.5, 1.0));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(15.0f / 255.0f, 15.0f / 255.0f, 15.0f / 255.0f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(7.0f / 255.0f, 7.0f / 255.0f, 47.0f / 255.0f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(3.0f / 255.0f, 3.0f / 255.0f, 62.0f / 255.0f, 1.0f));
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

        bool wasUsingGizmo = usingGizmo;
        usingGizmo = false;

        // draw gizmos and move entities
        if(editor.selectedIDs.size() > 1) {
            gizmoMode = ImGuizmo::MODE::WORLD;
            glm::vec3 centerOfSelection;
            glm::vec3 min { +INFINITY, +INFINITY, +INFINITY };
            glm::vec3 max { -INFINITY, -INFINITY, -INFINITY };
            Carrot::Vector<glm::vec3> offsetFromCenter;
            for(const auto& e : editor.selectedIDs) {
                auto transformRef = editor.currentScene.world.getComponent<Carrot::ECS::TransformComponent>(e);
                if(transformRef) {
                    glm::vec3 p = transformRef->computeFinalPosition();
                    min = glm::min(min, p);
                    max = glm::max(max, p);
                }
            }

            centerOfSelection = (min + max) / 2.0f;

            for(const auto& e : editor.selectedIDs) {
                auto transformRef = editor.currentScene.world.getComponent<Carrot::ECS::TransformComponent>(e);
                if(transformRef) {
                    glm::vec3 p = transformRef->computeFinalPosition();
                    offsetFromCenter.pushBack(p - centerOfSelection);
                } else {
                    offsetFromCenter.pushBack(glm::vec3{0.0f});
                }
            }

            glm::mat4 transformMatrix = glm::translate(glm::identity<glm::mat4>(), centerOfSelection);
            glm::mat4 deltaMatrix;

            bool used = ImGuizmo::Manipulate(
                    cameraViewImGuizmo,
                    cameraProjectionImGuizmo,
                    gizmoOperation,
                    gizmoMode,
                    glm::value_ptr(transformMatrix),
                    glm::value_ptr(deltaMatrix)
            );

            if (used) {
                float dtranslation[3] = {0.0f};
                float dscale[3] = {0.0f};
                float drotation[3] = {0.0f};
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(deltaMatrix), dtranslation, drotation,
                                                      dscale);

                for(const auto& e : editor.selectedIDs) {
                    auto transformRef = editor.currentScene.world.getComponent<Carrot::ECS::TransformComponent>(e);
                    if(transformRef) {
                        transformRef->localTransform.position += glm::vec3(dtranslation[0], dtranslation[1],
                                                   dtranslation[2]);

                        // TODO: find a graceful way of rotating and scaling multiple objects at once
                        /*
                        transformRef->localTransform.rotation = glm::quat(
                                glm::radians(glm::vec3(drotation[0], drotation[1], drotation[2]))) * transformRef->localTransform.rotation;

                        transformRef->localTransform.scale *= glm::vec3(dscale[0], dscale[1], dscale[2]);
                        */
                    }
                }

                editor.markDirty();
            }
            usingGizmo = ImGuizmo::IsUsing() || ImGuizmo::IsOver();

            if(!usingGizmo && wasUsingGizmo) {
                // TODO: undo command
            }
        } else if (editor.selectedIDs.size() == 1) {
            auto transformRef = editor.currentScene.world.getComponent<Carrot::ECS::TransformComponent>(editor.selectedIDs[0]);
            if (transformRef) {
                glm::mat4 parentMatrix = glm::identity<glm::mat4>();
                auto parentEntity = transformRef->getEntity().getParent();
                if (parentEntity) {
                    auto parentTransform = parentEntity->getComponent<Carrot::ECS::TransformComponent>();
                    if (parentTransform) {
                        parentMatrix = parentTransform->toTransformMatrix();

                        const glm::vec3 worldScale = parentTransform->computeFinalScale();
                        parentMatrix = parentMatrix * glm::scale(glm::mat4(1.0f), 1.0f / worldScale);
                    }
                }


                glm::mat4 transformMatrix = parentMatrix * transformRef->localTransform.toTransformMatrix();

                bool used = ImGuizmo::Manipulate(
                        cameraViewImGuizmo,
                        cameraProjectionImGuizmo,
                        gizmoOperation,
                        gizmoMode,
                        glm::value_ptr(transformMatrix)
                );

                if (used) {
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

                if(!usingGizmo && wasUsingGizmo) {
                    // TODO: undo command
                }
            }
        }
    }

    bool GizmosLayer::allowSceneEntityPicking() const {
        return !usingGizmo;
    }
} // Peeler