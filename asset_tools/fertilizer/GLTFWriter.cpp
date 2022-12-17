//
// Created by jglrxavpok on 17/12/2022.
//

#include "GLTFWriter.h"
#include "core/utils/stringmanip.h"

namespace Fertilizer {
    struct Payload {
        tinygltf::Model& glTFModel;

        std::unordered_map<Carrot::IO::VFS::Path, int> textureIndices;
    };

    static std::vector<double> vectorOfDoubles(const glm::vec2& v) {
        return { v.x, v.y };
    }

    static std::vector<double> vectorOfDoubles(const glm::vec3& v) {
        return { v.x, v.y, v.z };
    }

    static std::vector<double> vectorOfDoubles(const glm::vec4& v) {
        return { v.x, v.y, v.z, v.w };
    }

    /**
     * Writes samplers, images and textures to the glTF model, and fills 'payload' with indexing info for textures
     */
    static void writeTextures(Payload& payload, const Carrot::Render::LoadedScene& scene) {
        tinygltf::Model& model = payload.glTFModel;
        model.samplers.emplace_back(); // add a default sampler (unused by Carrot)

        // Carrot has 1:1 mapping between images and textures
        std::size_t textureCount = 0;
        auto addTexture = [&](const Carrot::IO::VFS::Path& path) {
            if(path.isEmpty()) {
                return;
            }
            verify(path.isGeneric(), "Fertilizer does not have a full VFS, all paths should be generic");

            auto [it, wasNew] = payload.textureIndices.insert({ path, textureCount });
            if(wasNew) {
                textureCount++;
            }
        };
        for(const auto& material : scene.materials) {
            addTexture(material.albedo);
            addTexture(material.occlusion);
            addTexture(material.normalMap);
            addTexture(material.metallicRoughness);
            addTexture(material.emissive);
        }

        model.images.resize(textureCount);
        model.textures.resize(textureCount);
        for(const auto& [path, index] : payload.textureIndices) {
            model.images[index].uri = path.toString();

            model.textures[index].name = path.toString(); // to help for debuggability
            model.textures[index].source = index;
        }
    }

    static tinygltf::TextureInfo makeTextureInfo(const Payload& payload, const Carrot::IO::VFS::Path& texturePath) {
        auto it = payload.textureIndices.find(texturePath);
        verify(it != payload.textureIndices.end(), "Programming error: a texture was referenced by scene, but not written to Payload");

        int index = it->second;
        tinygltf::TextureInfo r;
        r.index = index;
        return r;
    }

    static void writeMaterials(Payload& payload, const Carrot::Render::LoadedScene& scene) {
        tinygltf::Model& model = payload.glTFModel;
        std::size_t materialCount = scene.materials.size();
        model.materials.resize(materialCount);

        for (std::size_t i = 0; i < materialCount; ++i) {
            tinygltf::Material& glTFMaterial = model.materials[i];
            const Carrot::Render::LoadedMaterial& material = scene.materials[i];

            switch(material.blendMode) {
                case Carrot::Render::LoadedMaterial::BlendMode::Blend:
                    glTFMaterial.alphaMode = "BLEND";
                    break;

                case Carrot::Render::LoadedMaterial::BlendMode::None:
                    glTFMaterial.alphaMode = "OPAQUE";
                    break;

                default:
                    verify(false, Carrot::sprintf("Unsupported blend type: %d", material.blendMode)); // unsupported type
            }

            glTFMaterial.name = material.name;

            glTFMaterial.pbrMetallicRoughness.baseColorFactor = vectorOfDoubles(material.baseColorFactor);
            glTFMaterial.pbrMetallicRoughness.roughnessFactor = material.roughnessFactor;
            glTFMaterial.pbrMetallicRoughness.metallicFactor = material.metallicFactor;

            glTFMaterial.emissiveFactor = vectorOfDoubles(material.emissiveFactor);

            // unsupported by Carrot
            // TODO: show warning in GLTFLoader ?
            {
                glTFMaterial.normalTexture.scale;
                glTFMaterial.normalTexture.texCoord;
                glTFMaterial.occlusionTexture.texCoord;
            }

            glTFMaterial.pbrMetallicRoughness.baseColorTexture = makeTextureInfo(payload, material.albedo);
            glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture = makeTextureInfo(payload, material.metallicRoughness);
            glTFMaterial.emissiveTexture = makeTextureInfo(payload, material.emissive);
            glTFMaterial.normalTexture.index = makeTextureInfo(payload, material.normalMap).index;
            glTFMaterial.occlusionTexture.index = makeTextureInfo(payload, material.occlusion).index;
        }
    }

    static void writeMeshes(Payload& payload, const Carrot::Render::LoadedScene& scene) {
        TODO;
    }

    static void writeNodes(Payload& payload, const Carrot::Render::LoadedScene& scene) {
        TODO;
    }

    static void writeScenes(Payload& payload, const Carrot::Render::LoadedScene& scene) {
        TODO;
    }

    tinygltf::Model writeAsGLTF(const Carrot::Render::LoadedScene& scene) {
        tinygltf::Model model;

        model.asset.generator = "Fertilizer v1";
        model.asset.version = "2.0";

        Payload payload {
                .glTFModel = model,
        };

        writeTextures(payload, scene);
        writeMaterials(payload, scene);
        writeMeshes(payload, scene);
        writeNodes(payload, scene);
        writeScenes(payload, scene);

        // TODO

        return std::move(model);
    }
}