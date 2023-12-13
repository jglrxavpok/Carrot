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

    void editModelComponent(EditContext& edition, Carrot::ECS::ModelComponent* component) {
        auto& asyncModel = component->asyncModel;
        if(!asyncModel.isReady() && !asyncModel.isEmpty()) {
            ImGui::Text("Model is loading...");
            return;
        }
        std::string path = asyncModel.isEmpty() ? "" : asyncModel->getOriginatingResource().getName();
        if(ImGui::InputText("Filepath##ModelComponent filepath inspector", path, ImGuiInputTextFlags_EnterReturnsTrue)) {
            component->setFile(Carrot::IO::VFS::Path(path));
            edition.hasModifications = true;
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
                if(!std::filesystem::is_directory(fsPath) && Carrot::IO::isModelFormatFromPath(s.c_str())) {
                    component->setFile(vfsPath);
                    edition.hasModifications = true;
                }
            }

            ImGui::EndDragDropTarget();
        }

        ImGui::Checkbox("Transparent##ModelComponent transparent inspector", &component->isTransparent);
        ImGui::Checkbox("Casts shadows##ModelComponent casts shadows inspector", &component->castsShadows);

        float colorArr[4] = { component->color.r, component->color.g, component->color.b, component->color.a };
        if(ImGui::ColorPicker4("Model color", colorArr)) {
            component->color = glm::vec4 { colorArr[0], colorArr[1], colorArr[2], colorArr[3] };
            edition.hasModifications = true;
        }

        if(asyncModel.isReady() && ImGui::CollapsingHeader("Material overrides")) {
            auto& worldData = component->getEntity().getWorld().getWorldData();
            auto cloneRenderer = [&]() {
                if(component->modelRenderer) {
                    return component->modelRenderer->clone();
                }

                return std::make_shared<Carrot::Render::ModelRenderer>(*asyncModel);
            };

            std::size_t virtualGeometryCount = 0;
            std::size_t staticMeshCount = component->modelRenderer ? component->modelRenderer->getModel().getStaticMeshes().size() : component->asyncModel->getStaticMeshes().size();

            // handle rendering overrides (replacing pipeline and/or material textures)
            if(component->modelRenderer) {
                std::shared_ptr<Carrot::Render::ModelRenderer> clonedRenderer = nullptr;
                auto cloneIfNeeded = [&]() {
                    if(!clonedRenderer) {
                        clonedRenderer = cloneRenderer();
                    }

                    return clonedRenderer;
                };

                static std::string albedoPath = "<<path>>";

                Carrot::Render::MaterialSystem& materialSystem = GetRenderer().getMaterialSystem();
                std::size_t index = 0;
                const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
                const float spacing = ImGui::GetStyle().ItemSpacing.y;

                std::size_t toRemove = ~0ull;

                for(const auto& override : component->modelRenderer->getOverrides()) {
                    ImGui::BeginChild(Carrot::sprintf("Material overrides##%llu", index).c_str(), ImVec2(0, lineHeight * 9 + spacing*2), true);

                    bool removed = editSingleOverride(edition, override, *component->modelRenderer, cloneIfNeeded);
                    if(removed) {
                        toRemove = index;
                    }

                    if(override.virtualizedGeometry) {
                        virtualGeometryCount++;
                    }

                    ImGui::EndChild();
                    index++;
                }

                if(toRemove < component->modelRenderer->getOverrides().size()) {
                    cloneIfNeeded()->removeOverride(toRemove);
                }

                if(clonedRenderer) {
                    component->modelRenderer = clonedRenderer;
                    clonedRenderer->recreateStructures();
                    worldData.storeModelRenderer(clonedRenderer);
                }
            }

            if(ImGui::Button("+")) {
                component->modelRenderer = cloneRenderer();
                component->modelRenderer->addOverride({});
                worldData.storeModelRenderer(component->modelRenderer);
                edition.hasModifications = true;
            }

            int tristate = -1; // partially selected
            if(staticMeshCount == virtualGeometryCount) {
                tristate = 1;
            } else if(virtualGeometryCount == 0) {
                tristate = 0;
            }
            if(ImGui::CheckBoxTristate("Virtual Geometry for everything", &tristate)) {
                if(tristate == 0) {
                    if(component->modelRenderer) {
                        component->modelRenderer = cloneRenderer();
                        for(auto& override : component->modelRenderer->getOverrides()) {
                            override.virtualizedGeometry = false;
                        }
                        component->modelRenderer->recreateStructures();
                        worldData.storeModelRenderer(component->modelRenderer);
                    }
                } else if(tristate == 1) {
                    component->modelRenderer = cloneRenderer();
                    for(std::size_t i = 0; i < staticMeshCount; i++) {
                        Carrot::Render::MaterialOverride* pExistingOverride = component->modelRenderer->getOverrides().findForMesh(i);
                        if(pExistingOverride) {
                            pExistingOverride->virtualizedGeometry = true;
                        } else {
                            Carrot::Render::MaterialOverride override;
                            override.meshIndex = i;
                            override.virtualizedGeometry = true;
                            component->modelRenderer->addOverride(override);
                        }
                    }
                    component->modelRenderer->recreateStructures();
                    worldData.storeModelRenderer(component->modelRenderer);
                }
            }
        }
    }
}