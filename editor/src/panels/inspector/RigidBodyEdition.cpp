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
            if (editor.selectedIDs.empty()) {
                remove();
                return;
            }

            auto selectedEntity = editor.currentScene.world.wrap(editor.selectedIDs[0]);
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

    void editPhysicsCharacterComponent(EditContext& edition, Carrot::ECS::PhysicsCharacterComponent* component) {
        float mass = component->character.getMass();
        if(ImGui::DragFloat("Mass", &mass, 0.1f, 0.001f, FLT_MAX, "%.2f kg")) {
            component->character.setMass(mass);
        }

        Carrot::Physics::CollisionLayerID layerID = component->character.getCollisionLayer();
        auto& layersManager = GetPhysics().getCollisionLayers();
        const auto& currentLayer = layersManager.getLayer(layerID);

        bool changedLayer = false;
        if(ImGui::BeginCombo("Collision layer", currentLayer.name.c_str())) {
            for(const auto& layer : layersManager.getLayers()) {
                if(ImGui::Selectable(layer.name.c_str(), layer.layerID == layerID)) {
                    changedLayer = true;
                    layerID = layer.layerID;
                }
            }
            ImGui::EndCombo();
        }

        if(changedLayer) {
            edition.hasModifications = true;
            component->character.setCollisionLayer(layerID);
        }

        bool modified = drawColliderUI(edition, component->getEntity(), component->character.getCollider(), -1, nullptr);
        if(modified) {
            component->character.applyColliderChanges();
        }
    }

    void editRigidBodyComponent(EditContext& edition, Carrot::ECS::RigidBodyComponent* component) {
        auto& rigidbody = component->rigidbody;
        const BodyType type = rigidbody.getBodyType();

        if(ImGui::BeginCombo("Type##rigidbodycomponent", component->getTypeName(type))) {
            for(BodyType bodyType : { BodyType::Dynamic, BodyType::Kinematic, BodyType::Static }) {
                bool selected = bodyType == type;
                if(ImGui::Selectable(component->getTypeName(bodyType), selected)) {
                    rigidbody.setBodyType(bodyType);
                    edition.hasModifications = true;
                }
            }
            ImGui::EndCombo();
        }

        Carrot::Physics::CollisionLayerID layerID = component->rigidbody.getCollisionLayer();
        auto& layersManager = GetPhysics().getCollisionLayers();
        const auto& currentLayer = layersManager.getLayer(layerID);

        bool changedLayer = false;
        if(ImGui::BeginCombo("Collision layer", currentLayer.name.c_str())) {
            for(const auto& layer : layersManager.getLayers()) {
                if(ImGui::Selectable(layer.name.c_str(), layer.layerID == layerID)) {
                    changedLayer = true;
                    layerID = layer.layerID;
                }
            }
            ImGui::EndCombo();
        }

        if(changedLayer) {
            edition.hasModifications = true;
            component->rigidbody.setCollisionLayer(layerID);
        }

        // handle axis locking
        {
            glm::vec3 translationAxes = rigidbody.getTranslationAxes();
            glm::vec3 rotationAxes = rigidbody.getRotationAxes();
            auto showLockButton = [&](const char *id, glm::vec3& axes, std::uint8_t axisIndex) -> bool {
                const ImVec2 buttonSize{ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()};
                bool isLocked = glm::abs(axes[axisIndex]) < 0.01f;

                const auto textureRef = isLocked ? GetAssetServer().blockingLoadTexture(LockedWidgetTexture)
                                                 : GetAssetServer().blockingLoadTexture(UnlockedWidgetTexture);
                const ImTextureID textureID = textureRef->getImguiID();
                const bool changed = ImGui::ImageToggleButton(id, textureID, &isLocked, buttonSize);
                if (changed) {
                    axes[axisIndex] = isLocked ? 0.0f : 1.0f;
                }

                return changed;
            };

            ImGui::Text("Lock translation");
            bool translationAxesChanged = false;
            ImGui::SameLine();
            translationAxesChanged |= showLockButton("X##lock translate X", translationAxes, 0);
            ImGui::SameLine();
            translationAxesChanged |= showLockButton("Y##lock translate Y", translationAxes, 1);
            ImGui::SameLine();
            translationAxesChanged |= showLockButton("Z##lock translate Z", translationAxes, 2);
            if (translationAxesChanged) {
                rigidbody.setTranslationAxes(translationAxes);
            }

            ImGui::Text("Lock rotation");
            bool rotationAxesChanged = false;
            ImGui::SameLine();
            rotationAxesChanged |= showLockButton("X##lock rotation X", rotationAxes, 0);
            ImGui::SameLine();
            rotationAxesChanged |= showLockButton("Y##lock rotation Y", rotationAxes, 1);
            ImGui::SameLine();
            rotationAxesChanged |= showLockButton("Z##lock rotation Z", rotationAxes, 2);
            if (rotationAxesChanged) {
                rigidbody.setRotationAxes(rotationAxes);
            }
        }

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
    }
}