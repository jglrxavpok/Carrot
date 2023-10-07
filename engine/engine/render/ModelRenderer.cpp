//
// Created by jglrxavpok on 15/07/2023.
//

#include <utility>
#include "ModelRenderer.h"
#include <engine/assets/AssetServer.h>
#include <engine/console/RuntimeOption.hpp>
#include <engine/render/resources/SingleMesh.h>
#include <engine/render/Model.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/utils/Profiling.h>
#include <core/utils/JSON.h>
#include <core/data/Hashes.h>
#include <robin_hood.h>

Carrot::RuntimeOption DrawBoundingSpheres("Debug/Draw bounding spheres", false);
Carrot::RuntimeOption DisableFrustumCheck("Debug/Disable frustum culling", false);

namespace Carrot::Render {
    bool MaterialOverride::operator==(const MaterialOverride& other) const {
        return meshIndex == other.meshIndex
        && pipeline == other.pipeline
        && (materialTextures == nullptr) == (other.materialTextures == nullptr)
        && (materialTextures == nullptr || *materialTextures == *other.materialTextures);
    }

    std::size_t MaterialOverride::hash() const {
        std::size_t hash = robin_hood::hash_int(meshIndex);
        hash_combine(hash, (std::uint64_t) pipeline.get());
        if(materialTextures) {
            hash_combine(hash, materialTextures->hash());
        }

        return hash;
    }

    const MaterialOverride* MaterialOverrides::begin() const {
        return sortedOverrides.data();
    }

    const MaterialOverride* MaterialOverrides::end() const {
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

    ModelRenderer::ModelRenderer(Model& model): model(model) {
        opaqueMeshesPipeline = GetRenderer().getOrCreatePipeline("gBuffer");
        transparentMeshesPipeline = GetRenderer().getOrCreatePipeline("gBuffer-transparent");

        recreateStructures();
    }

    ModelRenderer::ModelRenderer(Model& model, ModelRenderer::NoInitTag): model(model) {}

    /* static */std::shared_ptr<ModelRenderer> ModelRenderer::fromJSON(const rapidjson::Value& json) {
        std::shared_ptr<Carrot::Model> model = GetAssetServer().blockingLoadModel(json["model"].GetString());
        std::shared_ptr<ModelRenderer> renderer = std::make_unique<ModelRenderer>(*model);

        Render::MaterialSystem& materialSystem = GetRenderer().getMaterialSystem();

        // modify 'overrides' directly not to call recreateStructures for each override
        for(const auto& overrideObj : json["overrides"].GetArray()) {
            MaterialOverride override;

            override.meshIndex = overrideObj["mesh_index"].GetUint64();

            if(overrideObj.HasMember("pipeline_name")) {
                override.pipeline = GetRenderer().getOrCreatePipelineFullPath(overrideObj["pipeline_name"].GetString());
            }
            if(overrideObj.HasMember("material")) {
                const auto& texturesObj = overrideObj["material"].GetObject();
                override.materialTextures = materialSystem.createMaterialHandle();

                if(const auto& element = texturesObj.FindMember("transparent"); element != texturesObj.MemberEnd()) {
                    override.materialTextures->isTransparent = element->value.GetBool();
                }
                if(const auto& element = texturesObj.FindMember("metallicness"); element != texturesObj.MemberEnd()) {
                    override.materialTextures->metallicFactor = element->value.GetFloat();
                }
                if(const auto& element = texturesObj.FindMember("roughness"); element != texturesObj.MemberEnd()) {
                    override.materialTextures->roughnessFactor = element->value.GetFloat();
                }
                if(const auto& element = texturesObj.FindMember("base_color"); element != texturesObj.MemberEnd()) {
                    override.materialTextures->baseColor = Carrot::JSON::read<4, float>(element->value);
                }
                if(const auto& element = texturesObj.FindMember("emissive_color"); element != texturesObj.MemberEnd()) {
                    override.materialTextures->emissiveColor = Carrot::JSON::read<3, float>(element->value);
                }

                auto loadTexture = [&](const rapidjson::Value& jsonElement) {
                    std::string_view texturePath { jsonElement.GetString(), jsonElement.GetStringLength() };

                    return materialSystem.createTextureHandle(GetAssetServer().blockingLoadTexture(Carrot::IO::VFS::Path { texturePath }));
                };
                if(const auto& element = texturesObj.FindMember("albedo_texture"); element != texturesObj.MemberEnd()) {
                    override.materialTextures->albedo = loadTexture(element->value);
                }
                if(const auto& element = texturesObj.FindMember("emissive_texture"); element != texturesObj.MemberEnd()) {
                    override.materialTextures->emissive = loadTexture(element->value);
                }
                if(const auto& element = texturesObj.FindMember("normalmap_texture"); element != texturesObj.MemberEnd()) {
                    override.materialTextures->normalMap = loadTexture(element->value);
                }
                if(const auto& element = texturesObj.FindMember("metallic_roughness_texture"); element != texturesObj.MemberEnd()) {
                    override.materialTextures->metallicRoughness = loadTexture(element->value);
                }
            }

            renderer->overrides.add(override);
        }

        renderer->recreateStructures();

        return renderer;
    }

    rapidjson::Value ModelRenderer::toJSON(rapidjson::Document::AllocatorType& allocator) {
        rapidjson::Value result { rapidjson::kObjectType };

        const Carrot::IO::Resource& source = model.getOriginatingResource();
        if(!source.isFile()) {
            return {};
        }

        rapidjson::Value overridesObj { rapidjson::kArrayType };

        for(const MaterialOverride& override : overrides) {
            rapidjson::Value overrideObj { rapidjson::kObjectType };

            overrideObj.AddMember("mesh_index", override.meshIndex, allocator);

            if(override.pipeline) {
                const Carrot::IO::Resource& pipelineSource = override.pipeline->getDescription().originatingResource;
                if(pipelineSource.isFile()) {
                    overrideObj.AddMember("pipeline_name", pipelineSource.getName(), allocator);
                }
            }

            if(override.materialTextures) {
                rapidjson::Value materialObj{ rapidjson::kObjectType };

                auto storeTexture = [&](const std::string& name, std::shared_ptr<Render::TextureHandle> texture) {
                    if(!texture) {
                        return;
                    }

                    const Carrot::IO::Resource& textureSource = texture->texture->getOriginatingResource();
                    if(!textureSource.isFile()) {
                        return;
                    }
                    materialObj.AddMember(rapidjson::Value{ name.c_str(), allocator }, rapidjson::Value{ textureSource.getName().c_str(), allocator }, allocator);
                };
                storeTexture("albedo_texture", override.materialTextures->albedo);
                storeTexture("emissive_texture", override.materialTextures->emissive);
                storeTexture("metallic_roughness_texture", override.materialTextures->metallicRoughness);
                storeTexture("normalmap_texture", override.materialTextures->normalMap);

                materialObj.AddMember("base_color", Carrot::JSON::write(override.materialTextures->baseColor, allocator), allocator);
                materialObj.AddMember("emissive_color", Carrot::JSON::write(override.materialTextures->emissiveColor, allocator), allocator);
                materialObj.AddMember("metallicness", override.materialTextures->metallicFactor, allocator);
                materialObj.AddMember("roughness", override.materialTextures->roughnessFactor, allocator);
                materialObj.AddMember("transparent", override.materialTextures->isTransparent, allocator);

                overrideObj.AddMember("material", materialObj, allocator);
            }

            overridesObj.PushBack(overrideObj, allocator);
        }

        result.AddMember("model", rapidjson::Value{ source.getName().c_str(), allocator }, allocator);
        result.AddMember("overrides", overridesObj, allocator);
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

            if(override.materialTextures) {
                clonedOverride.materialTextures = materialSystem.createMaterialHandle();

                *clonedOverride.materialTextures = *override.materialTextures;
            }

            cloned->overrides.add(clonedOverride);
        }
        cloned->recreateStructures();

        return cloned;
    }

    void ModelRenderer::render(const Render::Context& renderContext, const InstanceData& instanceData, Render::PassEnum renderPass) const {
        ZoneScoped;

        // TODO: support for skinned meshes
        for(const auto& bucket : buckets) {
            ZoneScopedN("per bucket");
            Render::Packet& renderPacket = GetRenderer().makeRenderPacket(renderPass, renderContext.viewport);
            renderPacket.pipeline = bucket.pipeline;

            renderPacket.drawCommands = bucket.drawCommands;

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
                vk::DrawIndexedIndirectCommand* pDrawCommand = &renderPacket.drawCommands[meshIndex];
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
        std::unordered_map<std::shared_ptr<Carrot::Pipeline>, PipelineBucket> perPipelineBuckets;
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
                    auto& bucket = perPipelineBuckets[pipeline];

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

                    auto& drawCommand = bucket.drawCommands.emplace_back();
                    drawCommand.instanceCount = 1;
                    drawCommand.firstInstance = meshIndex; // increment just before
                    drawCommand.firstIndex = meshInfo.startIndex;
                    drawCommand.indexCount = meshInfo.indexCount;
                    drawCommand.vertexOffset = meshInfo.startVertex;

                    bucket.meshes.emplace_back(std::move(renderingInfo));
                }
            }
        }

        // flatten list
        buckets.clear();
        buckets.reserve(perPipelineBuckets.size());
        for(auto& [pipeline, bucket] : perPipelineBuckets) {
            PipelineBucket& newBucket = buckets.emplace_back();
            newBucket = std::move(bucket);
            newBucket.pipeline = pipeline;
        }
    }
} // Carrot::Render