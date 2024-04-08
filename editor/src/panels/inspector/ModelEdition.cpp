//
// Created by jglrxavpok on 24/07/2023.
//

#include "EditorFunctions.h"
#include <engine/Engine.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/render/MaterialSystem.h>
#include <engine/edition/DragDropTypes.h>
#include <engine/render/resources/Mesh.h>
#include <engine/render/ModelRenderer.h>
#include <engine/ecs/components/ModelComponent.h>
#include <engine/ecs/World.h>
#include <engine/ecs/WorldData.h>

namespace Peeler {
    // return true if should be removed
    bool editSingleOverride(EditContext& edition, const Carrot::Render::MaterialOverride& override,
                            const Carrot::Render::ModelRenderer& modelRenderer,
                            std::function<std::shared_ptr<Carrot::Render::ModelRenderer>()> cloneIfNeeded) {
        auto& materialSystem = GetRenderer().getMaterialSystem();

        const std::vector<std::shared_ptr<Carrot::Mesh>>& staticMeshes = modelRenderer.getModel().getStaticMeshes();
        const std::string& selectedMeshName = staticMeshes[override.meshIndex]->getDebugName();
        if(ImGui::BeginCombo("Mesh selection", selectedMeshName.c_str())) {
            for(std::size_t i = 0; i < staticMeshes.size(); i++) {
                const std::string& meshName = staticMeshes[i]->getDebugName();
                if(ImGui::Selectable(Carrot::sprintf("%s##%llu", meshName.c_str(), i).c_str(), i == override.meshIndex)) {
                    auto clonedRenderer = cloneIfNeeded();
                    clonedRenderer->getOverrides().findForMesh(override.meshIndex)->meshIndex = i;
                    clonedRenderer->getOverrides().sort();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if(ImGui::Button("Remove")) {
            return true;
        }

        auto modifyTextures = [&]() {
            auto renderer = cloneIfNeeded();
            Carrot::Render::MaterialOverride* pNewOverride = renderer->getOverrides().findForMesh(override.meshIndex);
            if(!pNewOverride->materialTextures) {
                pNewOverride->materialTextures = materialSystem.createMaterialHandle();
            }

            return pNewOverride->materialTextures;
        };

#define getTexture(TEXTURE_NAME) (override.materialTextures ? (override.materialTextures-> TEXTURE_NAME ? override.materialTextures-> TEXTURE_NAME ->texture : nullptr) : nullptr)

        Carrot::Render::Texture::Ref albedoRef = getTexture(albedo);
        if(edition.inspector.drawPickTextureWidget("Albedo##editModelComponent", &albedoRef)) {
            modifyTextures()->albedo = materialSystem.createTextureHandle(albedoRef);
            edition.hasModifications = true;
        }

        Carrot::Render::Texture::Ref normalMapRef = getTexture(normalMap);
        if(edition.inspector.drawPickTextureWidget("Normal map##editModelComponent", &normalMapRef)) {
            modifyTextures()->normalMap = materialSystem.createTextureHandle(normalMapRef);
            edition.hasModifications = true;
        }

        Carrot::Render::Texture::Ref metallicRoughnessRef = getTexture(metallicRoughness);
        if(edition.inspector.drawPickTextureWidget("Metallic Roughness##editModelComponent", &metallicRoughnessRef)) {
            modifyTextures()->metallicRoughness = materialSystem.createTextureHandle(metallicRoughnessRef);
            edition.hasModifications = true;
        }

        Carrot::Render::Texture::Ref emissiveRef = getTexture(emissive);
        if(edition.inspector.drawPickTextureWidget("Emissive##editModelComponent", &emissiveRef)) {
            modifyTextures()->emissive = materialSystem.createTextureHandle(emissiveRef);
            edition.hasModifications = true;
        }

        std::shared_ptr<Carrot::Pipeline> pipeline = override.pipeline;
        if(edition.inspector.drawPickPipelineWidget("Rendering pipeline##editModelComponent", &pipeline)) {
            auto renderer = cloneIfNeeded();
            Carrot::Render::MaterialOverride* pNewOverride = renderer->getOverrides().findForMesh(override.meshIndex);
            pNewOverride->pipeline = pipeline;
            edition.hasModifications = true;
        }

        bool isVirtualizedGeometry = override.virtualizedGeometry;
        if(ImGui::Checkbox("Virtualized Geometry##editModelComponent", &isVirtualizedGeometry)) {
            auto renderer = cloneIfNeeded();
            Carrot::Render::MaterialOverride* pNewOverride = renderer->getOverrides().findForMesh(override.meshIndex);
            pNewOverride->virtualizedGeometry = isVirtualizedGeometry;
            edition.hasModifications = true;
        }

        return false;
    }

    void editModelComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::ModelComponent*>& components) {
        // check if all components have loaded their models
        for(std::int64_t i = 0; i < components.size(); i++) {
            auto& asyncModel = components[i]->asyncModel;
            if(!asyncModel.isReady() && !asyncModel.isEmpty()) {
                ImGui::Text("Loading model(s)...");
                return;
            }
        }

        ImGui::BeginGroup();
        CLEANUP({
            ImGui::EndGroup();
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
                    if(!std::filesystem::is_directory(fsPath) && Carrot::IO::isModelFormatFromPath(s.c_str())) {
                        for(auto& pComponent : components) {
                            pComponent->setFile(vfsPath);
                        }
                        edition.hasModifications = true;
                    }
                }

                ImGui::EndDragDropTarget();
            }
        });

        multiEditField(edition, "Filepath", components,
            +[](Carrot::ECS::ModelComponent& c) {
                return Carrot::IO::VFS::Path { c.asyncModel.isEmpty() ? "" : c.asyncModel->getOriginatingResource().getName() };
            },
            +[](Carrot::ECS::ModelComponent& c, const Carrot::IO::VFS::Path& path) {
                c.setFile(path);
            },
            Helpers::Limits<Carrot::IO::VFS::Path> {
                .validityChecker = [](const auto& path) { return Carrot::IO::isModelFormat(path.toString().c_str()); }
            }
        );

        if(ImGui::SmallButton("Reload")) {
            for(auto& pComponent : components) {
                const Carrot::IO::VFS::Path vfsPath { pComponent->asyncModel.isEmpty() ? "" : pComponent->asyncModel->getOriginatingResource().getName() };
                pComponent->setFile(vfsPath);
            }
            edition.hasModifications = true;
        }

        multiEditField(edition, "Transparent", components,
            +[](Carrot::ECS::ModelComponent& c) -> bool& { return c.isTransparent; });
        multiEditField(edition, "Casts shadows", components,
            +[](Carrot::ECS::ModelComponent& c) -> bool& { return c.castsShadows; });

        multiEditField(edition, "Model color", components,
            +[](Carrot::ECS::ModelComponent& c) { return Helpers::RGBAColorWrapper { c.color }; },
            +[](Carrot::ECS::ModelComponent& c, const Helpers::RGBAColorWrapper& v) { c.color = v.rgba; });

        for(const auto& pComponent : components) {
            if(!pComponent->asyncModel.isReady()) {
                return;
            }
        }
        if(ImGui::CollapsingHeader("Material overrides")) {
            auto& worldData = components[0]->getEntity().getWorld().getWorldData();

            bool allSame = true;
            Carrot::Render::ModelRenderer* pModelRenderer = components[0]->modelRenderer.get();
            for(std::int64_t i = 1; i < components.size(); i++) {
                allSame &= pModelRenderer == components[i]->modelRenderer.get();
            }

            auto cloneRenderer = [&](Carrot::ECS::ModelComponent& component) {
                if(pModelRenderer) {
                    return pModelRenderer->clone();
                }

                return std::make_shared<Carrot::Render::ModelRenderer>(*component.asyncModel);
            };
            if(allSame) {
                // handle rendering overrides (replacing pipeline and/or material textures)
                if(pModelRenderer) {
                    std::shared_ptr<Carrot::Render::ModelRenderer> clonedRenderer = nullptr;
                    auto cloneIfNeeded = [&]() {
                        if(!clonedRenderer) {
                            clonedRenderer = cloneRenderer(*components[0] /* all same, so can reuse this one */);
                        }

                        return clonedRenderer;
                    };

                    static std::string albedoPath = "<<path>>";

                    Carrot::Render::MaterialSystem& materialSystem = GetRenderer().getMaterialSystem();
                    std::size_t index = 0;
                    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
                    const float spacing = ImGui::GetStyle().ItemSpacing.y;

                    std::size_t toRemove = ~0ull;

                    for(const auto& override : pModelRenderer->getOverrides()) {
                        ImGui::BeginChild(Carrot::sprintf("Material overrides##%llu", index).c_str(), ImVec2(0, lineHeight * 9 + spacing*5), true);

                        bool removed = editSingleOverride(edition, override, *pModelRenderer, cloneIfNeeded);
                        if(removed) {
                            toRemove = index;
                        }

                        ImGui::EndChild();
                        index++;
                    }

                    if(toRemove < pModelRenderer->getOverrides().size()) {
                        cloneIfNeeded()->removeOverride(toRemove);
                    }

                    if(clonedRenderer) {
                        for(auto& pComponent : components) {
                            pComponent->modelRenderer = clonedRenderer;
                        }
                        clonedRenderer->recreateStructures();
                        worldData.storeModelRenderer(clonedRenderer);
                    }
                }

                if(ImGui::Button("+")) {
                    std::shared_ptr<Carrot::Render::ModelRenderer> pClone = cloneRenderer(*components[0]);
                    pClone->addOverride({});
                    for(auto& pComponent : components) {
                        pComponent->modelRenderer = pClone;
                    }
                    pClone->recreateStructures();
                    worldData.storeModelRenderer(pClone);
                    edition.hasModifications = true;
                }
            } else {
                ImGui::Text("Not all using the same renderer/model, cannot modify overrides with the current selection of entities.");
                pModelRenderer = nullptr;
            }

            bool allWithVirtualGeometry = true;
            bool allWithoutVirtualGeometry = true;
            for(const auto& pComponent : components) {
                std::size_t virtualGeometryCount = 0;

                for(auto& override : pComponent->modelRenderer->getOverrides()) {
                    if(override.virtualizedGeometry) {
                        virtualGeometryCount++;
                    }
                }

                std::size_t staticMeshCount = pComponent->modelRenderer ? pComponent->modelRenderer->getModel().getStaticMeshes().size() : pComponent->asyncModel->getStaticMeshes().size();
                if(staticMeshCount == virtualGeometryCount) {
                    allWithoutVirtualGeometry = false;
                } else if(virtualGeometryCount == 0) {
                    allWithVirtualGeometry = false;
                } else {
                    allWithVirtualGeometry = false;
                    allWithoutVirtualGeometry = false;
                }
            }

            int tristate = -1;
            if(allWithVirtualGeometry) {
                tristate = 1;
            } else if(allWithoutVirtualGeometry) {
                tristate = 0;
            }

            if(ImGui::CheckBoxTristate("Virtual Geometry for everything", &tristate)) {
                if(tristate == 0) {
                    for(const auto& pComponent : components) {
                        if(pComponent->modelRenderer) {
                            pComponent->modelRenderer = cloneRenderer(*pComponent);
                            for(auto& override : pComponent->modelRenderer->getOverrides()) {
                                override.virtualizedGeometry = false;
                            }
                            pComponent->modelRenderer->recreateStructures();
                            worldData.storeModelRenderer(pComponent->modelRenderer);
                        }
                    }
                } else if(tristate == 1) {
                    for(const auto& pComponent : components) {
                        pComponent->modelRenderer = cloneRenderer(*pComponent);
                        std::size_t staticMeshCount = pComponent->modelRenderer ? pComponent->modelRenderer->getModel().getStaticMeshes().size() : pComponent->asyncModel->getStaticMeshes().size();
                        for(std::size_t i = 0; i < staticMeshCount; i++) {
                            Carrot::Render::MaterialOverride* pExistingOverride = pComponent->modelRenderer->getOverrides().findForMesh(i);
                            if(pExistingOverride) {
                                pExistingOverride->virtualizedGeometry = true;
                            } else {
                                Carrot::Render::MaterialOverride override;
                                override.meshIndex = i;
                                override.virtualizedGeometry = true;
                                pComponent->modelRenderer->addOverride(override);
                            }
                        }
                        pComponent->modelRenderer->recreateStructures();
                        worldData.storeModelRenderer(pComponent->modelRenderer);
                    }
                }
            }
        }
    }
}