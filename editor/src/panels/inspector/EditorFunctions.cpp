//
// Created by jglrxavpok on 31/05/2023.
//

#include "EditorFunctions.h"
#include <engine/edition/DragDropTypes.h>
#include <engine/Engine.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/ecs/components/AnimatedModelComponent.h>
#include <engine/ecs/components/CameraComponent.h>
#include <engine/ecs/components/CSharpComponent.h>
#include <engine/ecs/components/ForceSinPosition.h>
#include <engine/ecs/components/Kinematics.h>
#include <engine/ecs/components/LightComponent.h>
#include <engine/ecs/components/ModelComponent.h>
#include <engine/ecs/components/NavMeshComponent.h>
#include <engine/ecs/components/RigidBodyComponent.h>
#include <engine/ecs/components/PhysicsCharacterComponent.h>
#include <engine/ecs/components/SoundListenerComponent.h>
#include <engine/ecs/components/SpriteComponent.h>
#include <engine/ecs/components/TextComponent.h>
#include <engine/ecs/components/TransformComponent.h>
#include <core/io/Logging.hpp>
#include <engine/render/ModelRenderer.h>
#include <IconsFontAwesome5.h>
#include <Peeler.h>
#include <core/allocators/StackAllocator.h>
#include <engine/ecs/Prefab.h>
#include <engine/ecs/components/ParticleEmitterComponent.h>
#include <engine/ecs/components/PrefabInstanceComponent.h>

namespace Peeler {
    template<typename ComponentType>
    void registerFunction(InspectorPanel& inspector, void(*editionFunction)(EditContext&, const Carrot::Vector<ComponentType*>&)) {
        inspector.registerComponentEditor(ComponentType::getID(), [editionFunction](EditContext& editContext, const Carrot::Vector<Carrot::ECS::Component*>& comps) {
            Carrot::Vector<ComponentType*> typedComponents;
            typedComponents.resize(comps.size());
            for(std::int64_t i = 0; i < typedComponents.size(); i++) {
                typedComponents[i] = dynamic_cast<ComponentType*>(comps[i]);
            }
            editionFunction(editContext, typedComponents);
        });
    }

    static void editTransformComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::TransformComponent*>& components) {
        multiEditField(edition, "Position", components,
                       +[](Carrot::ECS::TransformComponent& t) -> glm::vec3& { return t.localTransform.position; });
        multiEditField(edition, "Rotation", components,
                       +[](Carrot::ECS::TransformComponent& t) -> glm::quat& { return t.localTransform.rotation; });
        multiEditField(edition, "Scale", components,
                       +[](Carrot::ECS::TransformComponent& t) -> glm::vec3& { return t.localTransform.scale; });
    }

    static void editLightComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::LightComponent*>& components) {
        multiEditField(edition, "Enabled", components,
            +[](Carrot::ECS::LightComponent& c) -> bool { return (c.lightRef->light.flags & Carrot::Render::LightFlags::Enabled) != Carrot::Render::LightFlags::None; },
            +[](Carrot::ECS::LightComponent& c, const bool& newValue) { c.lightRef->light.flags = newValue ? Carrot::Render::LightFlags::Enabled : Carrot::Render::LightFlags::None; });

        multiEditEnumField(edition, "Light type", components,
            +[](Carrot::ECS::LightComponent& c) { return c.lightRef->light.type ; },
            +[](Carrot::ECS::LightComponent& c, const Carrot::Render::LightType& v) { c.lightRef->light.type = v; },
            Carrot::Render::Light::nameOf, { Carrot::Render::LightType::Point, Carrot::Render::LightType::Directional, Carrot::Render::LightType::Spot });

        multiEditField(edition, "Intensity", components,
            +[](Carrot::ECS::LightComponent& c) -> float& { return c.lightRef->light.intensity; });

        if(ImGui::CollapsingHeader("Parameters")) {
            bool allSameType = true;
            Carrot::Render::LightType lightType = components[0]->lightRef->light.type;
            for(std::size_t i = 1; i < components.size(); i++) {
                if(lightType != components[i]->lightRef->light.type) {
                    allSameType = false;
                    break;
                }
            }
            if(allSameType) {
                switch (lightType) {
                    case Carrot::Render::LightType::Spot: {
                        multiEditField(edition, "Cutoff angle", components,
                            +[](Carrot::ECS::LightComponent& c) { return Helpers::CosAngleWrapper { c.lightRef->light.spot.cutoffCosAngle }; },
                            +[](Carrot::ECS::LightComponent& c, const Helpers::CosAngleWrapper& v) { c.lightRef->light.spot.cutoffCosAngle = v.cosRadianValue; });
                        multiEditField(edition, "Outer cutoff angle", components,
                            +[](Carrot::ECS::LightComponent& c) { return Helpers::CosAngleWrapper { c.lightRef->light.spot.outerCutoffCosAngle }; },
                            +[](Carrot::ECS::LightComponent& c, const Helpers::CosAngleWrapper& v) { c.lightRef->light.spot.outerCutoffCosAngle = v.cosRadianValue; });
                    } break;

                    case Carrot::Render::LightType::Point: {
                        multiEditField(edition, "Constant attenuation", components,
                            +[](Carrot::ECS::LightComponent& c) -> float& { return c.lightRef->light.point.constantAttenuation; });
                        multiEditField(edition, "Linear attenuation", components,
                            +[](Carrot::ECS::LightComponent& c) -> float& { return c.lightRef->light.point.linearAttenuation; });
                        multiEditField(edition, "Quadratic attenuation", components,
                            +[](Carrot::ECS::LightComponent& c) -> float& { return c.lightRef->light.point.quadraticAttenuation; });
                    } break;

                    default: {
                        ImGui::Text("Unhandled light type :(");
                    } break;
                }
            } else {
                ImGui::Text("Lights are not all of same type, cannot edit them at the same time.");
            }
        }

        multiEditField(edition, "Light color", components,
            +[](Carrot::ECS::LightComponent& c) { return Helpers::RGBColorWrapper { .rgb = c.lightRef->light.color }; },
            +[](Carrot::ECS::LightComponent& c, const Helpers::RGBColorWrapper& v) { c.lightRef->light.color = v.rgb; });
    }

    static void editSpriteComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::SpriteComponent*>& components) {
        multiEditField(edition, "Sprite asset", components,
            +[](Carrot::ECS::SpriteComponent& c) -> Carrot::Render::Texture::Ref {
                return c.sprite ? c.sprite->getTextureRef() : nullptr;
            },
            +[](Carrot::ECS::SpriteComponent& c, const Carrot::Render::Texture::Ref& texture) {
                if(!c.sprite) {
                    c.sprite = std::make_shared<Carrot::Render::Sprite>(texture);
                } else {
                    c.sprite->setTexture(texture);
                }
            }
        );

        multiEditField(edition, "Min U", components,
            +[](Carrot::ECS::SpriteComponent& c) { return c.sprite ? c.sprite->getTextureRegion().getMinX() : 0.0f; },
            +[](Carrot::ECS::SpriteComponent& c, const float& v) {
                if(!c.sprite) return;
                auto& region = c.sprite->getTextureRegion();
                float minU = v;
                float minV = region.getMinY();
                float maxU = region.getMaxX();
                float maxV = region.getMaxY();
                region = Carrot::Math::Rect2Df(minU, minV, maxU, maxV);
            }
        );

        multiEditField(edition, "Min V", components,
            +[](Carrot::ECS::SpriteComponent& c) { return c.sprite ? c.sprite->getTextureRegion().getMinY() : 0.0f; },
            +[](Carrot::ECS::SpriteComponent& c, const float& v) {
                if(!c.sprite) return;
                auto& region = c.sprite->getTextureRegion();
                float minU = region.getMinX();
                float minV = v;
                float maxU = region.getMaxX();
                float maxV = region.getMaxY();
                region = Carrot::Math::Rect2Df(minU, minV, maxU, maxV);
            }
        );

        multiEditField(edition, "Max U", components,
            +[](Carrot::ECS::SpriteComponent& c) { return c.sprite ? c.sprite->getTextureRegion().getMaxX() : 1.0f; },
            +[](Carrot::ECS::SpriteComponent& c, const float& v) {
                if(!c.sprite) return;
                auto& region = c.sprite->getTextureRegion();
                float minU = region.getMinX();
                float minV = region.getMinY();
                float maxU = v;
                float maxV = region.getMaxY();
                region = Carrot::Math::Rect2Df(minU, minV, maxU, maxV);
            }
        );

        multiEditField(edition, "Max V", components,
            +[](Carrot::ECS::SpriteComponent& c) { return c.sprite ? c.sprite->getTextureRegion().getMaxY() : 1.0f; },
            +[](Carrot::ECS::SpriteComponent& c, const float& v) {
                if(!c.sprite) return;
                auto& region = c.sprite->getTextureRegion();
                float minU = region.getMinX();
                float minV = region.getMinY();
                float maxU = region.getMaxX();
                float maxV = v;
                region = Carrot::Math::Rect2Df(minU, minV, maxU, maxV);
            }
        );
    }

    void editModelComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::ModelComponent*>& components);
    void editAnimatedModelComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::AnimatedModelComponent*>& components);

    void editKinematicsComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::Kinematics*>& components) {
        multiEditField(edition, "Velocity", components,
            +[](Carrot::ECS::Kinematics& c) -> glm::vec3& { return c.velocity; });
    }

    void editForceSinPositionComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::ForceSinPosition*>& components) {
        multiEditField(edition, "Angular Frequency", components,
            +[](Carrot::ECS::ForceSinPosition& c) -> glm::vec3& { return c.angularFrequency; });
        multiEditField(edition, "Amplitude", components,
            +[](Carrot::ECS::ForceSinPosition& c) -> glm::vec3& { return c.amplitude; });
        multiEditField(edition, "Angular Offset", components,
            +[](Carrot::ECS::ForceSinPosition& c) -> glm::vec3& { return c.angularOffset; });
        multiEditField(edition, "Center Position", components,
            +[](Carrot::ECS::ForceSinPosition& c) -> glm::vec3& { return c.centerPosition; });
    }

    void editCameraComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::CameraComponent*>& components) {
        multiEditField(edition, "Primary", components,
            +[](Carrot::ECS::CameraComponent& c) -> bool& { return c.isPrimary; });

        Carrot::Vector<bool> isOrthoFields;
        isOrthoFields.resize(components.size());
        for(std::int64_t i = 0; i < components.size(); i++) {
            isOrthoFields[i] = components[i]->isOrthographic;
        }

        multiEditField(edition, "Orthographic", components,
            +[](Carrot::ECS::CameraComponent& c) -> bool& { return c.isOrthographic; });

        if(Helpers::all(isOrthoFields)) {
            multiEditField(edition, "Bounds", components,
                +[](Carrot::ECS::CameraComponent& c) -> glm::vec3& { return c.orthoSize; });
        } else if(!Helpers::any(isOrthoFields)) {
            multiEditField(edition, "FOV", components,
                +[](Carrot::ECS::CameraComponent& c) { return Helpers::AngleWrapper{ c.perspectiveFov }; },
                +[](Carrot::ECS::CameraComponent& c, const Helpers::AngleWrapper& v) { c.perspectiveFov = v.radianValue; },
                Helpers::Limits<Helpers::AngleWrapper> { .min = 0.0001f, .max = 360.0f });
            multiEditField(edition, "Z Near", components, +[](Carrot::ECS::CameraComponent& c) -> float& { return c.perspectiveNear; });
            multiEditField(edition, "Z Far", components, +[](Carrot::ECS::CameraComponent& c) -> float& { return c.perspectiveFar; });
        } else {
            ImGui::Text("Cannot edit mix of orthographic and perspective cameras at the same time.");
        }
    }

    void editTextComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::TextComponent*>& components) {
        multiEditField(edition, "Content", components,
            +[](Carrot::ECS::TextComponent& c) { return c.getText(); },
            +[](Carrot::ECS::TextComponent& c, const std::string_view& v) { c.setText(v); },
            Helpers::Limits<std::string_view> { .multiline = true });

        multiEditEnumField(edition, "Horizontal Alignment", components,
            +[](Carrot::ECS::TextComponent& c) { return c.getHorizontalAlignment(); },
            +[](Carrot::ECS::TextComponent& c, const Carrot::Render::TextAlignment& v) { c.setHorizontalAlignment(v); },
            +[](const Carrot::Render::TextAlignment& c) {
                switch (c) {
                    case Carrot::Render::TextAlignment::Left:
                        return "Left";
                    case Carrot::Render::TextAlignment::Center:
                        return "Center";
                    default:
                        TODO;
                }
                return "";
            },
            {Carrot::Render::TextAlignment::Left, Carrot::Render::TextAlignment::Center});

        multiEditField(edition, "Color", components,
            +[](Carrot::ECS::TextComponent& c) { return Helpers::RGBAColorWrapper(c.getColor()); },
            +[](Carrot::ECS::TextComponent& c, const Helpers::RGBAColorWrapper& v) { c.setColor(v.rgba); });
    }

    void editNavMeshComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::NavMeshComponent*>& components) {
        multiEditField(edition, "Mesh", components,
            +[](Carrot::ECS::NavMeshComponent& component) {
                return Carrot::IO::VFS::Path { component.meshFile.isFile() ? component.meshFile.getName() : "" };
            },
            +[](Carrot::ECS::NavMeshComponent& c, const Carrot::IO::VFS::Path& v) {
                c.setMesh(v);
            },
            Helpers::Limits<Carrot::IO::VFS::Path> {
                .validityChecker = [](auto& p) { return Carrot::IO::getFileFormat(p.toString().c_str()) == Carrot::IO::FileFormat::CNAV; }
            });
    }

    void editRigidBodyComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::RigidBodyComponent*>& components);
    void editPhysicsCharacterComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::PhysicsCharacterComponent*>& components);
    void editParticleEmitterComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::ParticleEmitterComponent*>& components);

    void editPrefabComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::PrefabInstanceComponent*>& components) {
        auto getPath = [&](std::int64_t index) -> Carrot::IO::VFS::Path {
            return components[index]->prefab ? components[index]->prefab->getVFSPath() : "";
        };
        Carrot::IO::VFS::Path pathToDisplay = getPath(0);
        bool samePath = true;
        for (int i = 1; i < components.size(); ++i) {
            if(getPath(i) != pathToDisplay) {
                samePath = false;
                break;
            }
        }

        if(samePath) {
            ImGui::Text("Prefab: %s", pathToDisplay.toString().c_str());
        } else {
            ImGui::TextWrapped("Selected entities are not instances of the same prefab, nothing interesting to show.");
        }
    }

    bool editVFSPath(const char* id, Carrot::IO::VFS::Path& path, const Helpers::Limits<Carrot::IO::VFS::Path>& limits) {
        std::string pathStr = path.toString();
        bool wasModified = false;
        if(ImGui::InputText(id, pathStr, ImGuiInputTextFlags_EnterReturnsTrue)) {
            wasModified = true;
        }

        if(ImGui::BeginDragDropTarget()) {
            if(auto* payload = ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::FilePath)) {
                std::unique_ptr<char8_t[]> buffer = std::make_unique<char8_t[]>(payload->DataSize+sizeof(char8_t));
                std::memcpy(buffer.get(), static_cast<const char8_t*>(payload->Data), payload->DataSize);
                buffer.get()[payload->DataSize] = '\0';

                std::u8string str = buffer.get();
                std::string s = Carrot::toString(str);

                auto vfsPath = Carrot::IO::VFS::Path(s);

                // TODO: no need to go through disk again
                std::filesystem::path fsPath = GetVFS().resolve(vfsPath);
                if((std::filesystem::is_directory(fsPath) == limits.allowDirectories) && limits.validityChecker(vfsPath)) {
                    pathStr = s;
                    wasModified = true;
                }
            }

            ImGui::EndDragDropTarget();
        }

        if (wasModified) {
            path = Carrot::IO::VFS::Path { pathStr };
        }
        return wasModified;
    }


    void registerEditionFunctions(InspectorPanel& inspector) {
        registerFunction(inspector, editCameraComponent);
        registerFunction(inspector, editForceSinPositionComponent);
        registerFunction(inspector, editKinematicsComponent);
        registerFunction(inspector, editLightComponent);
        registerFunction(inspector, editModelComponent);
        registerFunction(inspector, editAnimatedModelComponent);
        registerFunction(inspector, editRigidBodyComponent);
        registerFunction(inspector, editSpriteComponent);
        registerFunction(inspector, editTextComponent);
        registerFunction(inspector, editTransformComponent);
        registerFunction(inspector, editPhysicsCharacterComponent);
        registerFunction(inspector, editNavMeshComponent);
        registerFunction(inspector, editPrefabComponent);
        registerFunction(inspector, editParticleEmitterComponent);
    }

    void registerDisplayNames(InspectorPanel& inspector) {
        inspector.registerComponentDisplayName(Carrot::ECS::TransformComponent::getID(), ICON_FA_ARROWS_ALT "  Transform");
        inspector.registerComponentDisplayName(Carrot::ECS::LightComponent::getID(), ICON_FA_LIGHTBULB "  Light");
        inspector.registerComponentDisplayName(Carrot::ECS::ModelComponent::getID(), ICON_FA_CUBE "  Model");
        inspector.registerComponentDisplayName(Carrot::ECS::AnimatedModelComponent::getID(), ICON_FA_CUBE "  AnimatedModel");
        inspector.registerComponentDisplayName(Carrot::ECS::CameraComponent::getID(), ICON_FA_CAMERA_RETRO "  Camera");
        inspector.registerComponentDisplayName(Carrot::ECS::RigidBodyComponent::getID(), ICON_FA_CUBES "  Rigidbody");
        inspector.registerComponentDisplayName(Carrot::ECS::PhysicsCharacterComponent::getID(), ICON_FA_CHILD "  Physics Character");
        inspector.registerComponentDisplayName(Carrot::ECS::SpriteComponent::getID(), ICON_FA_IMAGE "  Sprite");
        inspector.registerComponentDisplayName(Carrot::ECS::TextComponent::getID(), ICON_FA_FONT "  Text");
        inspector.registerComponentDisplayName(Carrot::ECS::NavMeshComponent::getID(), ICON_FA_ROUTE "  NavMesh");
        inspector.registerComponentDisplayName(Carrot::ECS::SoundListenerComponent::getID(), ICON_FA_PODCAST "  SoundListener");
        inspector.registerComponentDisplayName(Carrot::ECS::PrefabInstanceComponent::getID(), ICON_FA_TOOLBOX "  PrefabInstance");
        inspector.registerComponentDisplayName(Carrot::ECS::ParticleEmitterComponent::getID(), ICON_FA_MAGIC "  ParticleEmitter");
    }
}
