//
// Created by jglrxavpok on 21/10/2021.
//
#include <stdexcept>
#include <iostream>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <engine/io/Resource.h>

int main() {
    Carrot::IO::Resource file = "resources/models/Television_01_4k.blend/Television_01_4k.fbx";
    Assimp::Importer importer{};
    const aiScene* scene = nullptr;
    {
        auto buffer = file.readAll();
        scene = importer.ReadFileFromMemory(buffer.get(), file.getSize(), aiProcess_Triangulate | aiProcess_OptimizeMeshes | aiProcess_FlipUVs);
    }

    if(!scene) {
        throw std::runtime_error("Failed to load model "+file.getName());
    }

    for(std::size_t materialIndex = 0; materialIndex < scene->mNumMaterials; materialIndex++) {
        auto* material = scene->mMaterials[materialIndex];
        std::string name = std::string(material->GetName().C_Str());
        std::cout << "Material " << name << std::endl;

        auto printTextureType = [&](aiTextureType type) {
            std::cout << "Texture type: " << TextureTypeToString(type) << std::endl;
            std::size_t count = material->GetTextureCount(type);
            std::cout << "Count: " << count << std::endl;
            for (std::size_t i = 0; i < count; ++i) {
                aiString path;
                material->GetTexture(type, i, &path);
                std::cout << "- [" << std::to_string(i) << "] = " << path.C_Str() << std::endl;
            }
        };

        printTextureType(aiTextureType_DIFFUSE);
        printTextureType(aiTextureType_AMBIENT);
        printTextureType(aiTextureType_BASE_COLOR);
        printTextureType(aiTextureType_NORMALS);

        aiString vertexShader;
        material->Get(AI_MATKEY_SHADER_VERTEX, vertexShader);
        std::cout << "Vertex shader = " << vertexShader.C_Str() << std::endl;

        aiString fragmentShader;
        material->Get(AI_MATKEY_SHADER_FRAGMENT, fragmentShader);
        std::cout << "Fragment shader = " << fragmentShader.C_Str() << std::endl;

        std::cout << "===========" << std::endl;
    }
    return 0;
}