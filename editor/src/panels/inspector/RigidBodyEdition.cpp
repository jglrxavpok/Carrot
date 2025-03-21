//
// Created by jglrxavpok on 03/06/2023.
//

#include "EditorFunctions.h"
#include <core/utils/ImGuiUtils.hpp>
#include <engine/Engine.h>
#include <engine/physics/PhysicsSystem.h>
#include <engine/edition/DragDropTypes.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/render/Model.h>
#include <engine/ecs/components/PhysicsCharacterComponent.h>
#include <engine/ecs/components/RigidBodyComponent.h>
#include <core/io/Logging.hpp>
#include <layers/ISceneViewLayer.h>
#include "../../Peeler.h"

#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <engine/ecs/components/TransformComponent.h>

using namespace Carrot::Physics;

namespace Peeler {

    static const char* EditColliderIcon = "resources/textures/ui/wrench.png";
    static const char* ResetWidgetTexture = "resources/textures/ui/reset.png";
    static const char* LockedWidgetTexture = "resources/textures/ui/locked.png";
    static const char* UnlockedWidgetTexture = "resources/textures/ui/unlocked.png";
    static const char* LockedVariousWidgetTexture = "resources/textures/ui/locked_various.png";

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

        bool editCollisionShapeWithGizmos(Carrot::ECS::Entity& selectedEntity, float startX, float startY, Collider& collider) {
            auto& shape = collider.getShape();
            auto localTransform = collider.getLocalTransform();
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

            if (auto selfTransform = selectedEntity.getComponent<Carrot::ECS::TransformComponent>()) {
                objectMatrix = selfTransform->toTransformMatrix();

                const glm::vec3 worldScale = selfTransform->computeFinalScale();
                objectMatrix = objectMatrix * glm::scale(glm::mat4(1.0f), 1.0f / worldScale);
            }

            switch (shape.getType()) {
                case ColliderType::Box: {
                    localTransform.scale = dynamic_cast<BoxCollisionShape&>(shape).getHalfExtents();
                }
                    break;

                case ColliderType::Sphere: {
                    localTransform.scale = glm::vec3(
                            dynamic_cast<SphereCollisionShape&>(shape).getRadius());
                }
                    break;

                case ColliderType::Capsule: {
                    auto& capsule = dynamic_cast<CapsuleCollisionShape&>(shape);
                    localTransform.scale = glm::vec3(capsule.getRadius(), capsule.getHeight(), capsule.getRadius());
                }
                    break;

                default:
                    TODO; // not implemented yet
            }

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
            DISCARD(used);

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

            // handle scaling
            {
                const glm::vec3 scaleDiff = glm::vec3(scale[0], scale[1], scale[2]) - localTransform.scale;
                const bool scalingX = glm::abs(scaleDiff.x) >= 10e-6f;
                const bool scalingY = glm::abs(scaleDiff.y) >= 10e-6f;
                const bool scalingZ = glm::abs(scaleDiff.z) >= 10e-6f;
                const bool isScaling = scalingX || scalingY || scalingZ;

                // scale is ignored by physics
                if (isScaling) {
                    switch (shape.getType()) {
                        case ColliderType::Box: {
                            dynamic_cast<BoxCollisionShape&>(shape).setHalfExtents(
                                    glm::vec3(scale[0], scale[1], scale[2]));
                        }
                            break;

                        case ColliderType::Sphere: {
                            float radius = scalingX ? scale[0]
                                                    : scalingY ? scale[1]
                                                               : scale[2];
                            dynamic_cast<SphereCollisionShape&>(shape).setRadius(radius);
                        }
                            break;
                        case ColliderType::Capsule: {
                            if (scalingX || scalingZ) {
                                dynamic_cast<CapsuleCollisionShape&>(shape).setRadius(
                                        scalingX ? scale[0] : scale[2]);
                            } else {
                                verify(scalingY, "Must be scaling at least once axis to end up here!");
                                dynamic_cast<CapsuleCollisionShape&>(shape).setHeight(scale[1]);
                            }
                        }
                            break;

                        default:
                            TODO; // not implemented yet
                    }
                }

                if(used) {
                    collider.setLocalTransform(localTransform);
                    editor.markDirty();
                }
                return used;
            }
        }

        virtual void draw(const Carrot::Render::Context& renderContext, float startX, float startY) override final {
            // make sure we are still editing the entity we think we are
            if (editor.selectedEntityIDs.empty()) {
                remove();
                return;
            }

            auto selectedEntity = editor.currentScene.world.wrap(editor.selectedEntityIDs[0]);
            if (!selectedEntity) {
                remove();
                return;
            }

            if (selectedEntity.getID() != targetEntity) {
                remove();
                return;
            }

            if(targetColliderIndex == -1) { // PhysicsCharacter
                auto physicsCharacterComp = selectedEntity.getComponent<Carrot::ECS::PhysicsCharacterComponent>();
                if (!physicsCharacterComp.hasValue()) {
                    remove();
                    return;
                }

                if(editCollisionShapeWithGizmos(selectedEntity, startX, startY, physicsCharacterComp->character.getCollider())) {
                    physicsCharacterComp->character.applyColliderChanges();
                }
            } else {
                auto rigidBodyComp = selectedEntity.getComponent<Carrot::ECS::RigidBodyComponent>();
                if (!rigidBodyComp.hasValue()) {
                    remove();
                    return;
                }

                if (rigidBodyComp->rigidbody.getColliderCount() <= targetColliderIndex) {
                    remove();
                    return;
                }

                // edit the selected collider
                auto& collider = rigidBodyComp->rigidbody.getCollider(targetColliderIndex);

                editCollisionShapeWithGizmos(selectedEntity, startX, startY, collider);
            }
        }

    private:
        Carrot::ECS::EntityID targetEntity;
        std::size_t targetColliderIndex = 0;
    };

    static bool ResetButton(const char* id) {
        ImGui::PushID(id);
        float buttonSize = ImGui::GetTextLineHeight();
        auto texture = GetAssetServer().blockingLoadTexture(ResetWidgetTexture);
        bool result = ImGui::ImageButton(texture->getImguiID(), ImVec2(buttonSize, buttonSize));
        ImGui::PopID();
        return result;
    }

    static Carrot::Physics::Collider* currentlyEditedCollider = nullptr;

    static bool drawColliderUI(EditContext& edition, Carrot::ECS::Entity& entity, Carrot::Physics::Collider& collider, std::size_t index, bool* removed) {
        ImGui::PushID(&collider);
        ImGui::Text("%s", Carrot::Physics::ColliderTypeNames[collider.getType()]);

        if(removed) {
            ImGui::SameLine();
            *removed = ImGui::Button("Remove");
        }

        bool modified = false;

        bool editLocalTransform = currentlyEditedCollider == &collider;
        float buttonSize = ImGui::GetTextLineHeight();
        auto texture = GetAssetServer().blockingLoadTexture(EditColliderIcon);
        if(ImGui::ImageToggleButton("Edit collider", texture->getImguiID(), &editLocalTransform, ImVec2(buttonSize, buttonSize))) {
            bool hasLayer = edition.editor.hasLayer<ColliderEditionLayer>();
            if(editLocalTransform) {
                currentlyEditedCollider = &collider;
                if(!hasLayer) {
                    edition.editor.pushLayer<ColliderEditionLayer>(entity, index);
                }
            } else {
                currentlyEditedCollider = nullptr;
                if(hasLayer) {
                    edition.editor.removeLayer<ColliderEditionLayer>();
                }
            }
        }

        auto& shape = collider.getShape();
        bool localTransformUpdated = false;
        Carrot::Math::Transform localTransform = collider.getLocalTransform();
        {
            float arr[] = { localTransform.position.x, localTransform.position.y, localTransform.position.z };
            if (ImGui::DragFloat3("Position", arr)) {
                localTransform.position = { arr[0], arr[1], arr[2] };
                edition.hasModifications = true;
                localTransformUpdated = true;
            }

            ImGui::SameLine();
            if(ResetButton("Position")) {
                localTransform.position = glm::vec3 { 0.0f };
                edition.hasModifications = true;
                localTransformUpdated = true;
            }
        }

        {
            float arr[] = { localTransform.rotation.x, localTransform.rotation.y, localTransform.rotation.z, localTransform.rotation.w };
            if (ImGui::DragFloat4("Rotation", arr)) {
                localTransform.rotation = { arr[3], arr[0], arr[1], arr[2] };
                edition.hasModifications = true;
                localTransformUpdated = true;
            }

            ImGui::SameLine();
            if(ResetButton("Rotation")) {
                localTransform.rotation = glm::identity<glm::quat>();
                edition.hasModifications = true;
                localTransformUpdated = true;
            }
        }

        if(localTransformUpdated) {
            collider.setLocalTransform(localTransform);
            modified = true;
        }

        switch(shape.getType()) {
            case Carrot::Physics::ColliderType::Sphere: {
                auto& sphere = static_cast<Carrot::Physics::SphereCollisionShape&>(shape);
                float radius = sphere.getRadius();
                if(ImGui::DragFloat("Radius", &radius, 0.1f, 0.001f)) {
                    if(radius <= 0) {
                        radius = 10e-6f;
                    }
                    edition.hasModifications = true;
                    modified = true;
                    sphere.setRadius(radius);
                }
            }
                break;

            case Carrot::Physics::ColliderType::Box: {
                auto& box = static_cast<Carrot::Physics::BoxCollisionShape&>(shape);

                float halfExtentsArray[3] = { box.getHalfExtents().x, box.getHalfExtents().y, box.getHalfExtents().z };
                if(ImGui::SliderFloat3("Half extents", halfExtentsArray, 0.001f, 100)) {
                    edition.hasModifications = true;
                    modified = true;
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
                    modified = true;
                    capsule.setRadius(radius);
                }
                if(ImGui::DragFloat("Height", &height, 0.1f, 0.001f)) {
                    if(height <= 0) {
                        height = 10e-6f;
                    }
                    edition.hasModifications = true;
                    modified = true;
                    capsule.setHeight(height);
                }
            }
                break;

            default:
                TODO
                break;
        }

        ImGui::PopID();

        return modified;
    }

    void editPhysicsCharacterComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::PhysicsCharacterComponent*>& components) {
        multiEditField(edition, "Mass", components,
            +[](Carrot::ECS::PhysicsCharacterComponent& c) {
                return c.character.getMass();
            },
            +[](Carrot::ECS::PhysicsCharacterComponent& c, const float& mass) {
                c.character.setMass(mass);
            },
            Helpers::Limits<float> {
                .formatOverride = "%.2f kg"
            });

        Carrot::Vector<CollisionLayerID> validIDs;
        const std::vector<CollisionLayer>& layers = GetPhysics().getCollisionLayers().getLayers();
        validIDs.ensureReserve(layers.size());
        for(const auto& layer : layers) {
            validIDs.pushBack(layer.layerID);
        }
        multiEditEnumField(edition, "Collision layer", components,
            +[](Carrot::ECS::PhysicsCharacterComponent& c) {
                return c.character.getCollisionLayer();
            },
            +[](Carrot::ECS::PhysicsCharacterComponent& c, const CollisionLayerID& v) {
                c.character.setCollisionLayer(v);
            },
            +[](const CollisionLayerID& c) { return GetPhysics().getCollisionLayers().getLayer(c).name.c_str(); },
            validIDs
        );

        multiEditField(edition, "Apply rotation", components, +[](Carrot::ECS::PhysicsCharacterComponent& c) -> bool& {
            return c.applyRotation;
        });

        if(components.size() == 1) {
            auto& component = components[0];
            bool modified = drawColliderUI(edition, component->getEntity(), component->character.getCollider(), -1, nullptr);
            if(modified) {
                component->character.applyColliderChanges();
            }
        } else {
            ImGui::Text("Edition of colliders is not supported for multiple selected entities.");
        }
    }

    /// Wrapper for vec3 to allow edition of locked axes
    struct AxisLockWrapper {
        glm::vec3 axes;

        bool operator==(const AxisLockWrapper &) const = default;
        bool operator!=(const AxisLockWrapper &) const = default;
    };

    template<>
    static bool editMultiple<AxisLockWrapper>(const char* id, std::span<AxisLockWrapper> values, const Helpers::Limits<AxisLockWrapper>& limits) {
        enum class State {
            LOCKED,
            UNLOCKED,
            VARIOUS // happens when multiple entities are selected but have different lock values
        };
        ImGui::PushID(id);
        auto showLockButton = [&](const char *id, State* pTristate) -> bool {
            const ImVec2 buttonSize{ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};

            Carrot::Render::Texture::Ref textureRef;
            switch(*pTristate) {
                case State::LOCKED:
                    textureRef = GetAssetServer().blockingLoadTexture(LockedWidgetTexture);
                    break;

                case State::UNLOCKED:
                    textureRef = GetAssetServer().blockingLoadTexture(UnlockedWidgetTexture);
                    break;

                default:
                    textureRef = GetAssetServer().blockingLoadTexture(LockedVariousWidgetTexture);
                    break; // TODO
            }
            const ImTextureID textureID = textureRef->getImguiID();
            bool wasLocked = *pTristate != State::UNLOCKED;
            const bool changed = ImGui::ImageToggleButton(id, textureID, &wasLocked, buttonSize);
            if(ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                switch(*pTristate) {
                    case State::LOCKED:
                        ImGui::Text("Locked");
                    break;

                    case State::UNLOCKED:
                        ImGui::Text("Unlocked");
                    break;

                    case State::VARIOUS:
                        ImGui::Text("Various (multiple selected entities with different values)");
                    break;
                }
                ImGui::EndTooltip();
            }
            if(changed) {
                *pTristate = wasLocked ? State::UNLOCKED : State::LOCKED;
            }
            return changed;
        };

        ImGui::TextUnformatted(id);

        const char* names[3] = { "X", "Y", "Z" };
        bool modified = false;
        for(int i = 0; i < 3; i++) {
            ImGui::SameLine();
            State tristate = glm::abs(values[0].axes[i]) < 0.01f ? State::LOCKED : State::UNLOCKED;
            for(int j = 1; j < values.size(); j++) {
                State stateForCurrentValue = glm::abs(values[j].axes[i]) < 0.01f ? State::LOCKED : State::UNLOCKED;
                if(stateForCurrentValue != tristate) {
                    tristate = State::VARIOUS;
                    break;
                }
            }

            if(showLockButton(names[i], &tristate)) {
                const bool isLocked = tristate == State::LOCKED;
                for(AxisLockWrapper& axisLock : values) {
                    axisLock.axes[i] = isLocked ? 0.0f : 1.0f;
                    modified = true;
                }
            }
        }
        ImGui::PopID();
        return modified;
    }

    void editRigidBodyComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::RigidBodyComponent*>& components) {
        multiEditEnumField(edition, "Type", components,
            +[](Carrot::ECS::RigidBodyComponent& c) {
                return c.rigidbody.getBodyType();
            },
            +[](Carrot::ECS::RigidBodyComponent& c, const BodyType& v) {
                c.rigidbody.setBodyType(v);
            },
            Carrot::ECS::RigidBodyComponent::getTypeName,
            { BodyType::Dynamic, BodyType::Kinematic, BodyType::Static }
        );

        Carrot::Vector<CollisionLayerID> validIDs;
        const std::vector<CollisionLayer>& layers = GetPhysics().getCollisionLayers().getLayers();
        validIDs.ensureReserve(layers.size());
        for(const auto& layer : layers) {
            validIDs.pushBack(layer.layerID);
        }
        multiEditEnumField(edition, "Collision layer", components,
            +[](Carrot::ECS::RigidBodyComponent& c) {
                return c.rigidbody.getCollisionLayer();
            },
            +[](Carrot::ECS::RigidBodyComponent& c, const CollisionLayerID& v) {
                c.rigidbody.setCollisionLayer(v);
            },
            +[](const CollisionLayerID& c) { return GetPhysics().getCollisionLayers().getLayer(c).name.c_str(); },
            validIDs
        );

        // handle axis locking
        {
            multiEditField(edition, "Lock translation", components,
                +[](Carrot::ECS::RigidBodyComponent& c) {
                    return AxisLockWrapper { c.rigidbody.getTranslationAxes() };
                },
                +[](Carrot::ECS::RigidBodyComponent& c, const AxisLockWrapper& v) {
                    c.rigidbody.setTranslationAxes(v.axes);
                });
            multiEditField(edition, "Lock rotation", components,
               +[](Carrot::ECS::RigidBodyComponent& c) {
                   return AxisLockWrapper { c.rigidbody.getRotationAxes() };
               },
               +[](Carrot::ECS::RigidBodyComponent& c, const AxisLockWrapper& v) {
                   c.rigidbody.setRotationAxes(v.axes);
               });
        }

        if(components.size() == 1) {
            // TODO: undo commands
            auto& component = components[0];
            auto& rigidbody = component->rigidbody;

            if(ImGui::BeginChild("Colliders##rigidbodycomponent", ImVec2(), true)) {
                for(std::size_t index = 0; index < rigidbody.getColliderCount(); index++) {
                    if(index != 0) {
                        ImGui::Separator();
                    }
                    bool removed;
                    edition.hasModifications |= drawColliderUI(edition, component->getEntity(), rigidbody.getCollider(index), index, &removed);
                    if(removed) {
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
                    // TODO: convex

                    ImGui::EndMenu();
                }
            }
            ImGui::EndChild();
        } else {
            ImGui::Text("Edition of colliders is not supported for multiple selected entities.");
        }
    }
}