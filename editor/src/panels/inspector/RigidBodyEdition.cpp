//
// Created by jglrxavpok on 03/06/2023.
//

#include "EditorFunctions.h"
#include <engine/Engine.h>
#include <engine/edition/DragDropTypes.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/render/Model.h>
#include <engine/ecs/components/RigidBodyComponent.h>
#include <core/io/Logging.hpp>
#include <layers/ISceneViewLayer.h>
#include "../../Peeler.h"

#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <engine/ecs/components/TransformComponent.h>

namespace Peeler {
    class ColliderEditionLayer: public ISceneViewLayer {
    public:
        explicit ColliderEditionLayer(Application& editor, Carrot::ECS::EntityID targetEntity, std::size_t targetColliderIndex)
            : ISceneViewLayer(editor), targetEntity(targetEntity), targetColliderIndex(targetColliderIndex) {

        }

        bool allowCameraMovement() const override {
            return true;
        }

        bool showLayersBelow() const override {
            return false;
        }

        bool allowSceneEntityPicking() const override {
            return false;
        }

        virtual void draw(const Carrot::Render::Context& renderContext, float startX, float startY) override final {
            // make sure we are still editing the entity we think we are
            if(editor.selectedIDs.empty()) {
                remove();
                return;
            }

            auto selectedEntity = editor.currentScene.world.wrap(editor.selectedIDs[0]);
            if(!selectedEntity) {
                remove();
                return;
            }

            if(selectedEntity.getID() != targetEntity) {
                remove();
                return;
            }

            auto rigidBodyComp = selectedEntity.getComponent<Carrot::ECS::RigidBodyComponent>();
            if(!rigidBodyComp.hasValue()) {
                remove();
                return;
            }

            if(rigidBodyComp->rigidbody.getColliderCount() <= targetColliderIndex) {
                remove();
                return;
            }

            // edit the selected collider
            auto& collider = rigidBodyComp->rigidbody.getCollider(targetColliderIndex);

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

            glm::mat4 objectMatrix = glm::identity<glm::mat4>();
            auto parentEntity = selectedEntity.getParent();

            if(auto selfTransform = selectedEntity.getComponent<Carrot::ECS::TransformComponent>()) {
                objectMatrix = selfTransform->toTransformMatrix();
            }

            Carrot::Math::Transform localTransform = collider.getLocalTransform(); // what we are trying to edit

            glm::mat4 transformMatrix = objectMatrix * localTransform.toTransformMatrix();
            ImGuizmo::MODE gizmoMode = ImGuizmo::MODE::LOCAL;
            ImGuizmo::OPERATION gizmoOperation = ImGuizmo::OPERATION::UNIVERSAL;

            // let user edit local transform
            bool used = ImGuizmo::Manipulate(
                    cameraViewImGuizmo,
                    cameraProjectionImGuizmo,
                    gizmoOperation,
                    gizmoMode,
                    glm::value_ptr(transformMatrix)
            );

            // user has modified transform, update local transform
            if (used) {
                glm::mat4 localTransformMatrix = glm::inverse(objectMatrix) * transformMatrix;
                float translation[3] = {0.0f};
                float scale[3] = {0.0f};
                float rotation[3] = {0.0f};
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localTransformMatrix), translation, rotation,
                                                      scale);

                localTransform.position = glm::vec3(translation[0], translation[1],
                                                                  translation[2]);
                localTransform.rotation = glm::quat(
                        glm::radians(glm::vec3(rotation[0], rotation[1], rotation[2])));
                localTransform.scale = glm::vec3(scale[0], scale[1], scale[2]);
                collider.setLocalTransform(localTransform);

                editor.markDirty();
            }
        }

    private:
        Carrot::ECS::EntityID targetEntity;
        std::size_t targetColliderIndex = 0;
    };

    void editRigidBodyComponent(EditContext& edition, Carrot::ECS::RigidBodyComponent* component) {
        auto& rigidbody = component->rigidbody;
        const reactphysics3d::BodyType type = rigidbody.getBodyType();

        if(ImGui::BeginCombo("Type##rigidbodycomponent", component->getTypeName(type))) {
            for(reactphysics3d::BodyType bodyType : { reactphysics3d::BodyType::DYNAMIC, reactphysics3d::BodyType::KINEMATIC, reactphysics3d::BodyType::STATIC }) {
                bool selected = bodyType == type;
                if(ImGui::Selectable(component->getTypeName(bodyType), selected)) {
                    rigidbody.setBodyType(bodyType);
                    edition.hasModifications = true;
                }
            }
            ImGui::EndCombo();
        }

        auto drawColliderUI = [&](Carrot::Physics::Collider& collider, std::size_t index) {
            ImGui::PushID(&collider);
            ImGui::Text("%s", Carrot::Physics::ColliderTypeNames[collider.getType()]);
            ImGui::SameLine();
            bool shouldRemove = ImGui::Button("Remove collider");

            static bool editLocalTransform = false;
            if(ImGui::Checkbox("Edit local transform", &editLocalTransform)) {
                bool hasLayer = edition.editor.hasLayer<ColliderEditionLayer>();
                if(editLocalTransform) {
                    if(!hasLayer) {
                        edition.editor.pushLayer<ColliderEditionLayer>(component->getEntity(), index);
                    }
                } else {
                    if(hasLayer) {
                        edition.editor.removeLayer<ColliderEditionLayer>();
                    }
                }
            }

            auto& shape = collider.getShape();
            switch(shape.getType()) {
                case Carrot::Physics::ColliderType::Sphere: {
                    auto& sphere = static_cast<Carrot::Physics::SphereCollisionShape&>(shape);
                    float radius = sphere.getRadius();
                    if(ImGui::DragFloat("Radius", &radius, 0.1f, 0.001f)) {
                        if(radius <= 0) {
                            radius = 10e-6f;
                        }
                        edition.hasModifications = true;
                        sphere.setRadius(radius);
                    }
                }
                    break;

                case Carrot::Physics::ColliderType::Box: {
                    auto& box = static_cast<Carrot::Physics::BoxCollisionShape&>(shape);

                    float halfExtentsArray[3] = { box.getHalfExtents().x, box.getHalfExtents().y, box.getHalfExtents().z };
                    if(ImGui::SliderFloat3("Half extents", halfExtentsArray, 0.001f, 100)) {
                        edition.hasModifications = true;
                        box.setHalfExtents({ halfExtentsArray[0], halfExtentsArray[1], halfExtentsArray[2] });
                    }
                }
                    break;

                case Carrot::Physics::ColliderType::Capsule: {
                    auto& capsule = static_cast<Carrot::Physics::CapsuleCollisionShape&>(shape);

                    float radius = capsule.getRadius();
                    float height = capsule.getHeight();
                    if(ImGui::DragFloat("Radius", &radius, 0.1f, 0.001f)) {
                        if(radius <= 0) {
                            radius = 10e-6f;
                        }
                        edition.hasModifications = true;
                        capsule.setRadius(radius);
                    }
                    if(ImGui::DragFloat("Height", &height, 0.1f, 0.001f)) {
                        if(height <= 0) {
                            height = 10e-6f;
                        }
                        edition.hasModifications = true;
                        capsule.setHeight(height);
                    }
                }
                    break;

                case Carrot::Physics::ColliderType::StaticConcaveTriangleMesh: {
                    auto& meshShape = static_cast<Carrot::Physics::StaticConcaveMeshCollisionShape&>(shape);

                    auto setModel = [&](const std::string& modelPath) {
                        auto model = GetRenderer().getOrCreateModel(modelPath);
                        if(model) {
                            meshShape.setModel(model);
                            edition.hasModifications = true;
                        } else {
                            Carrot::Log::error("Could not open model for collisions: %s", modelPath.c_str());
                        }
                    };
                    std::string path = meshShape.getModel().getOriginatingResource().getName();
                    if(ImGui::InputText("Filepath##ModelComponent filepath inspector", path, ImGuiInputTextFlags_EnterReturnsTrue)) {
                        setModel(path);
                    }

                    if(ImGui::BeginDragDropTarget()) {
                        if(auto* payload = ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::FilePath)) {
                            std::unique_ptr<char8_t[]> buffer = std::make_unique<char8_t[]>(payload->DataSize+sizeof(char8_t));
                            std::memcpy(buffer.get(), static_cast<const char8_t*>(payload->Data), payload->DataSize);
                            buffer.get()[payload->DataSize] = '\0';

                            std::filesystem::path newPath = buffer.get();

                            std::filesystem::path fsPath = std::filesystem::proximate(newPath, std::filesystem::current_path());
                            if(!std::filesystem::is_directory(fsPath) && Carrot::IO::isModelFormatFromPath(fsPath)) {
                                setModel(Carrot::toString(fsPath.u8string()));
                            }
                        }

                        ImGui::EndDragDropTarget();
                    }
                }
                    break;

                default:
                    TODO
                    break;
            }

            ImGui::PopID();

            return shouldRemove;
        };

        if(ImGui::BeginChild("Colliders##rigidbodycomponent", ImVec2(), true)) {
            for(std::size_t index = 0; index < rigidbody.getColliderCount(); index++) {
                if(index != 0) {
                    ImGui::Separator();
                }
                bool remove = drawColliderUI(rigidbody.getCollider(index), index);
                if(remove) {
                    rigidbody.removeCollider(index);
                    index--;
                    edition.hasModifications = true;
                }
            }

            ImGui::Separator();

            if(ImGui::BeginMenu("Add##rigidbodycomponent colliders")) {
                if(ImGui::MenuItem("Sphere Collider##rigidbodycomponent colliders")) {
                    rigidbody.addCollider(Carrot::Physics::SphereCollisionShape(1.0f));
                    edition.hasModifications = true;
                }
                if(ImGui::MenuItem("Box Collider##rigidbodycomponent colliders")) {
                    rigidbody.addCollider(Carrot::Physics::BoxCollisionShape(glm::vec3(0.5f)));
                    edition.hasModifications = true;
                }
                if(ImGui::MenuItem("Capsule Collider##rigidbodycomponent colliders")) {
                    rigidbody.addCollider(Carrot::Physics::CapsuleCollisionShape(1.0f, 1.0f));
                    edition.hasModifications = true;
                }
                if(ImGui::MenuItem("Static Concave Mesh Collider##rigidbodycomponent colliders")) {
                    rigidbody.addCollider(Carrot::Physics::StaticConcaveMeshCollisionShape(GetRenderer().getOrCreateModel("resources/models/simple_cube.obj")));
                    edition.hasModifications = true;
                }
                // TODO: convex

                ImGui::EndMenu();
            }
        }
        ImGui::EndChild();
    }
}