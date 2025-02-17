//
// Created by jglrxavpok on 15/07/2023.
//

#include <utility>
#include "ModelRenderer.h"
#include <engine/assets/AssetServer.h>
#include <engine/console/RuntimeOption.hpp>
#include <engine/render/resources/SingleMesh.h>
#include <engine/render/Model.h>
#include <engine/render/ClusterManager.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/utils/Profiling.h>
#include <core/io/DocumentHelpers.h>
#include <core/data/Hashes.h>
#include <robin_hood.h>

Carrot::RuntimeOption DrawBoundingSpheres("Debug/Draw bounding spheres", false);
Carrot::RuntimeOption DisableFrustumCheck("Debug/Disable frustum culling", false);

namespace Carrot::Render {
    bool MaterialOverride::operator==(const MaterialOverride& other) const {
        return meshIndex == other.meshIndex
        && pipeline == other.pipeline
        && virtualizedGeometry == other.virtualizedGeometry
        && (materialTextures == nullptr) == (other.materialTextures == nullptr)
        && (materialTextures == nullptr || *materialTextures == *other.materialTextures);
    }

    std::size_t MaterialOverride::hash() const {
        std::size_t hash = robin_hood::hash_int(meshIndex);
        hash_combine(hash, (std::uint64_t) pipeline.get());
        if(materialTextures) {
            hash_combine(hash, materialTextures->hash());
        }
        hash_combine(hash, robin_hood::hash_int(virtualizedGeometry ? 1 : 0));

        return hash;
    }

    const MaterialOverride* MaterialOverrides::begin() const {
        return sortedOverrides.data();
    }

    const MaterialOverride* MaterialOverrides::end() const {
        return sortedOverrides.data() + sortedOverrides.size();
    }

    MaterialOverride* MaterialOverrides::begin() {
        return sortedOverrides.data();
    }

    MaterialOverride* MaterialOverrides::end() {
        return sortedOverrides.data() + sortedOverrides.size();
    }

    std::size_t MaterialOverrides::size() const {
        return sortedOverrides.size();
    }

    void MaterialOverrides::add(const MaterialOverride& override) {
        for (std::size_t i = 0; i < sortedOverrides.size(); i++) {
            MaterialOverride& o = sortedOverrides[i];
            if(o.meshIndex > override.meshIndex) {
                sortedOverrides.insert(sortedOverrides.begin() + i, override);
                return;
            }
        }

        // no override with index >= to the one we are adding
        sortedOverrides.push_back(override);
    }

    void MaterialOverrides::remove(std::size_t index) {
        sortedOverrides.erase(sortedOverrides.begin() + index);
    }

    bool MaterialOverrides::operator==(const MaterialOverrides& other) const {
        if(sortedOverrides.size() != other.sortedOverrides.size()) {
            return false;
        }

        for (std::size_t i = 0; i < sortedOverrides.size(); ++i) {
            if(sortedOverrides[i] != other.sortedOverrides[i]) {
                return false;
            }
        }
        return true;
    }

    std::size_t MaterialOverrides::hash() const {
        std::size_t hash = 0;
        for(const MaterialOverride& override : sortedOverrides) {
            hash_combine(hash, override.hash());
        }
        return hash;
    }

    MaterialOverride* MaterialOverrides::findForMesh(std::size_t meshIndex) {
        for(MaterialOverride& override : sortedOverrides) {
            if(override.meshIndex == meshIndex) {
                return &override;
            }
        }

        return nullptr;
    }

    const MaterialOverride* MaterialOverrides::findForMesh(std::size_t meshIndex) const {
        for(const MaterialOverride& override : *this) {
            if(override.meshIndex == meshIndex) {
                return &override;
            }
        }

        return nullptr;
    }

    void MaterialOverrides::sort() {
        std::sort(sortedOverrides.begin(), sortedOverrides.end(), [](MaterialOverride& a, MaterialOverride& b) {
            if(a.meshIndex == b.meshIndex) {
                return 0;
            }

            return a.meshIndex < b.meshIndex ? -1 : 1;
        });
    }

    ModelRendererStorage ModelRendererStorage::clone() const {
        ModelRendererStorage r;
        r.pCreator = pCreator;
        for(auto& [k, v] : clusterModelsPerViewport) {
            r.clusterModelsPerViewport[k] = v ? v->clone() : nullptr;
        }
        return r;
    }

    ModelRenderer::ModelRenderer(Model& model): model(model) {
        opaqueMeshesPipeline = GetRenderer().getOrCreatePipeline("gBuffer");
        transparentMeshesPipeline = GetRenderer().getOrCreatePipeline("gBuffer-transparent");

        recreateStructures();
    }

    ModelRenderer::ModelRenderer(Model& model, ModelRenderer::NoInitTag): model(model) {}

    /* static */std::shared_ptr<ModelRenderer> ModelRenderer::deserialise(const Carrot::DocumentElement& doc) {
        std::shared_ptr<Carrot::Model> model = GetAssetServer().blockingLoadModel(doc["model"].getAsString());
        std::shared_ptr<ModelRenderer> renderer = std::make_unique<ModelRenderer>(*model);

        Render::MaterialSystem& materialSystem = GetRenderer().getMaterialSystem();

        // modify 'overrides' directly not to call recreateStructures for each override
        for(const auto& overrideObj : doc["overrides"].getAsArray()) {
            MaterialOverride override;

            override.meshIndex = overrideObj["mesh_index"].getAsInt64();

            if(overrideObj.contains("pipeline_name")) {
                override.pipeline = GetRenderer().getOrCreatePipelineFullPath(std::string{overrideObj["pipeline_name"].getAsString()});
            }
            if(overrideObj.contains("material")) {
                const auto& texturesObj = overrideObj["material"].getAsObject();
                override.materialTextures = materialSystem.createMaterialHandle();

                if(const auto& element = texturesObj.find("transparent"); element != texturesObj.end()) {
                    override.materialTextures->isTransparent = element->second.getAsBool();
                }
                if(const auto& element = texturesObj.find("metallicness"); element != texturesObj.end()) {
                    override.materialTextures->metallicFactor = element->second.getAsDouble();
                }
                if(const auto& element = texturesObj.find("roughness"); element != texturesObj.end()) {
                    override.materialTextures->roughnessFactor = element->second.getAsDouble();
                }
                if(const auto& element = texturesObj.find("base_color"); element != texturesObj.end()) {
                    override.materialTextures->baseColor = Carrot::DocumentHelpers::read<4, float>(element->second);
                }
                if(const auto& element = texturesObj.find("emissive_color"); element != texturesObj.end()) {
                    override.materialTextures->emissiveColor = Carrot::DocumentHelpers::read<3, float>(element->second);
                }

                auto loadTexture = [&](const Carrot::DocumentElement& element) {
                    std::string_view texturePath = element.getAsString();

                    return materialSystem.createTextureHandle(GetAssetServer().blockingLoadTexture(Carrot::IO::VFS::Path { texturePath }));
                };
                if(const auto& element = texturesObj.find("albedo_texture"); element != texturesObj.end()) {
                    override.materialTextures->albedo = loadTexture(element->second);
                }
                if(const auto& element = texturesObj.find("emissive_texture"); element != texturesObj.end()) {
                    override.materialTextures->emissive = loadTexture(element->second);
                }
                if(const auto& element = texturesObj.find("normalmap_texture"); element != texturesObj.end()) {
                    override.materialTextures->normalMap = loadTexture(element->second);
                }
                if(const auto& element = texturesObj.find("metallic_roughness_texture"); element != texturesObj.end()) {
                    override.materialTextures->metallicRoughness = loadTexture(element->second);
                }
            }
            if(overrideObj.contains("virtualized_geometry")) {
                override.virtualizedGeometry = overrideObj["virtualized_geometry"].getAsBool();
            }

            renderer->overrides.add(override);
        }

        renderer->recreateStructures();

        return renderer;
    }

    Carrot::DocumentElement ModelRenderer::serialise() {
        Carrot::DocumentElement result;

        const Carrot::IO::Resource& source = model.getOriginatingResource();
        if(!source.isFile()) {
            return Carrot::DocumentElement{};
        }

        Carrot::DocumentElement overridesObj{ Carrot::DocumentType::Array };

        for(const MaterialOverride& override : overrides) {
            Carrot::DocumentElement overrideObj;

            overrideObj["mesh_index"] = static_cast<i64>(override.meshIndex);

            if(override.pipeline) {
                const Carrot::IO::Resource& pipelineSource = override.pipeline->getDescription().originatingResource;
                if(pipelineSource.isFile()) {
                    overrideObj["pipeline_name"] = pipelineSource.getName();
                }
            }

            if(override.materialTextures) {
                Carrot::DocumentElement materialObj;

                auto storeTexture = [&](const std::string& name, std::shared_ptr<Render::TextureHandle> texture) {
                    if(!texture) {
                        return;
                    }

                    if(!texture->texture) {
                        return;
                    }

                    const Carrot::IO::Resource& textureSource = texture->texture->getOriginatingResource();
                    if(!textureSource.isFile()) {
                        return;
                    }
                    materialObj[name] = textureSource.getName();
                };
                storeTexture("albedo_texture", override.materialTextures->albedo);
                storeTexture("emissive_texture", override.materialTextures->emissive);
                storeTexture("metallic_roughness_texture", override.materialTextures->metallicRoughness);
                storeTexture("normalmap_texture", override.materialTextures->normalMap);

                materialObj["base_color"] = Carrot::DocumentHelpers::write(override.materialTextures->baseColor);
                materialObj["emissive_color"] = Carrot::DocumentHelpers::write(override.materialTextures->emissiveColor);
                materialObj["metallicness"] = override.materialTextures->metallicFactor;
                materialObj["roughness"] = override.materialTextures->roughnessFactor;
                materialObj["transparent"] = override.materialTextures->isTransparent;

                overrideObj["material"] = materialObj;
            }
            overrideObj["virtualized_geometry"] = override.virtualizedGeometry;

            overridesObj.pushBack() = overrideObj;
        }

        result["model"] = source.getName();
        result["overrides"] = overridesObj;
        return result;
    }

    std::shared_ptr<ModelRenderer> ModelRenderer::clone() const {
        auto cloned = std::make_shared<ModelRenderer>(model, NoInitTag{});
        cloned->opaqueMeshesPipeline = opaqueMeshesPipeline;
        cloned->transparentMeshesPipeline = transparentMeshesPipeline;

        MaterialSystem& materialSystem = GetRenderer().getMaterialSystem();
        for(const auto& override : overrides) {
            MaterialOverride clonedOverride{};

            clonedOverride.meshIndex = override.meshIndex;
            clonedOverride.pipeline = override.pipeline;
            clonedOverride.virtualizedGeometry = override.virtualizedGeometry;

            if(override.materialTextures) {
                clonedOverride.materialTextures = materialSystem.createMaterialHandle();

                *clonedOverride.materialTextures = *override.materialTextures;
            }

            cloned->overrides.add(clonedOverride);
        }
        cloned->recreateStructures();

        return cloned;
    }

    void ModelRenderer::render(ModelRendererStorage& storage, const Render::Context& renderContext, const InstanceData& instanceData, Render::PassName renderPass) const {
        ZoneScoped;

        if(storage.pCreator != this) {
            storage = {};
            storage.pCreator = this;

            // create clusters instances
            if(hasVirtualizedGeometry) {
                ClusterManager& meshletManager = renderContext.renderer.getMeshletManager();
                ClustersInstanceDescription instanceDesc;
                instanceDesc.pViewport = renderContext.pViewport;
                std::vector<std::shared_ptr<ClustersTemplate>> templates;
                std::vector<std::shared_ptr<MaterialHandle>> materials;
                std::vector<std::unordered_map<std::uint32_t, const PrecomputedBLAS*>> precomputedBLASes;

                for(const auto& bucket : buckets) {
                    if(!bucket.virtualizedGeometry) {
                        continue;
                    }

                    for(const auto& meshInfo : bucket.meshes) {
                        materials.push_back(meshInfo.materialTextures);
                        templates.push_back(model.lazyLoadMeshletTemplate(meshInfo.meshAndTransform.staticMeshIndex, meshInfo.meshAndTransform.transform));

                        std::unordered_map<std::uint32_t, const PrecomputedBLAS*> precomputedBLASesForThisMesh;
                        auto& precomputedBLASesForNode = model.getPrecomputedBLASes(meshInfo.meshAndTransform.nodeKey);
                        for(const auto& [blasKey, precomputedBlas] : precomputedBLASesForNode) {
                            if(blasKey.first == meshInfo.meshAndTransform.staticMeshIndex) {
                                precomputedBLASesForThisMesh[blasKey.second] = &precomputedBlas;
                            }
                        }
                        precomputedBLASes.push_back(std::move(precomputedBLASesForThisMesh));
                    }
                }
                instanceDesc.templates = templates;
                instanceDesc.pMaterials = materials;
                instanceDesc.precomputedBLASes = precomputedBLASes;

                auto clusterInstance = meshletManager.addModel(instanceDesc);
                storage.clusterModelsPerViewport[renderContext.pViewport] = clusterInstance;
            }
        }

        // activate and set instance data of meshlets
        auto iter = storage.clusterModelsPerViewport.find(renderContext.pViewport);
        if(iter != storage.clusterModelsPerViewport.end()) {
            auto& pInstance = iter->second;
            if(pInstance) {
                pInstance->enabled = true;
                pInstance->instanceData = instanceData;
            }
        }

        // TODO: support for skinned meshes
        for(const auto& bucket : buckets) {
            if(bucket.virtualizedGeometry) {
                continue;
            }
            ZoneScopedN("per bucket");
            Render::Packet& renderPacket = GetRenderer().makeRenderPacket(renderPass, Render::PacketType::DrawIndexedInstanced, renderContext);
            renderPacket.pipeline = bucket.pipeline;

            renderPacket.commands = bucket.drawCommands;

            renderPacket.vertexBuffer = model.getStaticMeshData().getVertexBuffer();
            renderPacket.indexBuffer = model.getStaticMeshData().getIndexBuffer();

            renderPacket.addPerDrawData(std::span(bucket.drawData));
            std::vector<InstanceData> instancesData = bucket.instanceData; // copied because modified below

            for (const auto& meshInfo: bucket.meshes) {
                auto& mesh = meshInfo.meshAndTransform.mesh;
                auto& transform = meshInfo.meshAndTransform.transform;
                auto& sphere = meshInfo.meshAndTransform.boundingSphere;
                auto& meshIndex = meshInfo.meshAndTransform.meshIndex;
                ZoneScopedN("mesh use");

                InstanceData* pInstanceData = &instancesData[meshIndex];
                vk::DrawIndexedIndirectCommand* pDrawCommand = &renderPacket.commands[meshIndex].drawIndexedInstanced;
                *pInstanceData = instanceData;

                pInstanceData->transform = instanceData.transform * transform;

                Math::Sphere s = sphere;
                s.transform(pInstanceData->transform);

                bool frustumCheck = renderContext.getCamera().isInFrustum(s);
                if(DisableFrustumCheck) {
                    frustumCheck = true;
                }

                pDrawCommand->instanceCount = frustumCheck ? 1 : 0;
                if(!frustumCheck) {
                    continue;
                }

                if(DrawBoundingSpheres) {
                    if(&model != renderContext.renderer.getUnitSphere().get()) {
                        glm::mat4 sphereTransform = glm::translate(glm::mat4{1.0f}, s.center) * glm::scale(glm::mat4{1.0f}, glm::vec3{s.radius*2 /*unit sphere model has a radius of 0.5*/});
                        renderContext.renderer.renderWireframeSphere(renderContext, sphereTransform, 1.0f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), instanceData.uuid);
                    }
                }

                pInstanceData->lastFrameTransform = instanceData.lastFrameTransform * transform;
            }

            renderPacket.useInstances(std::span(instancesData));

            renderContext.renderer.render(renderPacket);
        }
    }

    void ModelRenderer::addOverride(const MaterialOverride& override) {
        overrides.add(override);
        recreateStructures();
    }

    void ModelRenderer::removeOverride(std::size_t index) {
        overrides.remove(index);
        recreateStructures();
    }

    MaterialOverrides& ModelRenderer::getOverrides() {
        return overrides;
    }

    const MaterialOverrides& ModelRenderer::getOverrides() const {
        return overrides;
    }

    Carrot::Model& ModelRenderer::getModel() {
        return model;
    }

    const Carrot::Model& ModelRenderer::getModel() const {
        return model;
    }

    void ModelRenderer::recreateStructures() {
        struct BucketKey {
            std::shared_ptr<Carrot::Pipeline> pipeline;
            bool virtualizedGeometry = false;

            bool operator==(const BucketKey& other) const {
                return pipeline == other.pipeline
                    && virtualizedGeometry == other.virtualizedGeometry;
            }

            struct Hasher {
                std::size_t operator()(const BucketKey& k) const {
                    std::size_t hash = robin_hood::hash_int((std::uint64_t)k.pipeline.get());
                    hash_combine(hash, k.virtualizedGeometry ? 1ull : 0ull);
                    return hash;
                }
            };
        };

        std::unordered_map<BucketKey, PipelineBucket, BucketKey::Hasher> perPipelineBuckets;
        auto& materialSystem = GetRenderer().getMaterialSystem();
        for(auto& [materialSlot, meshList] : model.getStaticMeshesPerMaterial()) {
            for(auto& meshAndTransform : meshList) {

                // change pipeline and/or material textures based on user provided overrides
                const MaterialOverride* pOverride = overrides.findForMesh(meshAndTransform.staticMeshIndex);

                std::shared_ptr<MaterialHandle> pMat;
                std::shared_ptr<Pipeline> pipeline;

                if(pOverride) {
                    pMat = pOverride->materialTextures;
                    pipeline = pOverride->pipeline;
                }

                // no override, or override does not modify textures
                if(!pMat) {
                    auto materialHandle = materialSystem.getMaterial(materialSlot);
                    pMat = materialHandle.lock();
                }

                // no override, or override does not modify pipeline
                if(!pipeline) {
                    pipeline = pMat->isTransparent ? transparentMeshesPipeline : opaqueMeshesPipeline;
                }

                if(pMat) {
                    BucketKey key { pipeline, pOverride ? pOverride->virtualizedGeometry : false };
                    auto& bucket = perPipelineBuckets[key];

                    const Model::StaticMeshInfo& meshInfo = model.getStaticMeshInfo(meshAndTransform.staticMeshIndex);

                    MeshRenderingInfo renderingInfo {
                        .meshAndTransform = meshAndTransform,
                        .materialTextures = pMat,
                    };

                    const std::size_t meshIndex = bucket.meshes.size();
                    renderingInfo.meshAndTransform.meshIndex = meshIndex;

                    bucket.instanceData.emplace_back();

                    auto& drawData = bucket.drawData.emplace_back();
                    drawData.materialIndex = pMat->getSlot();

                    auto& drawCommand = bucket.drawCommands.emplace_back().drawIndexedInstanced;
                    drawCommand.instanceCount = 1;
                    drawCommand.firstInstance = meshIndex; // increment just before
                    drawCommand.firstIndex = meshInfo.startIndex;
                    drawCommand.indexCount = meshInfo.indexCount;
                    drawCommand.vertexOffset = meshInfo.startVertex;

                    bucket.meshes.emplace_back(std::move(renderingInfo));
                }
            }
        }

        hasVirtualizedGeometry = false;
        // flatten list
        buckets.clear();
        buckets.reserve(perPipelineBuckets.size());
        for(auto& [key, bucket] : perPipelineBuckets) {
            auto& [pipeline, virtualizedGeometry] = key;
            PipelineBucket& newBucket = buckets.emplace_back();
            newBucket = std::move(bucket);
            newBucket.pipeline = pipeline;
            newBucket.virtualizedGeometry = virtualizedGeometry;
            hasVirtualizedGeometry |= virtualizedGeometry;
        }
    }
} // Carrot::Render