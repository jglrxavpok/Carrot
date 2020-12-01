//
// Created by jglrxavpok on 30/11/2020.
//

#include "Model.h"
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/material.h>
#include "render/Mesh.h"
#include "render/Material.h"
#include <iostream>

Carrot::Model::Model(Carrot::Engine& engine, const string& filename): engine(engine) {
    Assimp::Importer importer{};
    const aiScene* scene = importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_OptimizeMeshes | aiProcess_FlipUVs);

    if(!scene) {
        throw runtime_error("Failed to load model "+filename);
    }


    for(size_t materialIndex = 0; materialIndex < scene->mNumMaterials; materialIndex++) {
        auto material = make_shared<Material>();
        const aiMaterial* mat = scene->mMaterials[materialIndex];

        int diffuseTextureCount = mat->GetTextureCount(aiTextureType_DIFFUSE);
        if(diffuseTextureCount == 1) {
            aiString name;
            mat->GetTexture(aiTextureType_DIFFUSE, 0, &name);
            cout << "Texture is " << name.data << endl;
        } else {
            cerr << "(Material " << mat->GetName().data << ") Unsupported diffuse texture count: " << diffuseTextureCount << ". Ignoring." << endl;
        }
        // TODO: build material
        materials.push_back(material);
        meshes[material.get()] = {};
    }

    for(size_t meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        unsigned int materialIndex = mesh->mMaterialIndex;
        auto material = materials[materialIndex];

        vector<Vertex> vertices{};
        vector<uint32_t> indices{};

        for(size_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; vertexIndex++) {
            const aiVector3D vec = mesh->mVertices[vertexIndex];
            const aiColor4D* vertexColor = mesh->mColors[0];
            const aiVector3D* vertexUVW = mesh->mTextureCoords[0];
            glm::vec3 color = {1.0f, 1.0f, 1.0f};
            if(vertexColor) {
                color = {
                        vertexColor[vertexIndex].r,
                        vertexColor[vertexIndex].g,
                        vertexColor[vertexIndex].b,
                };
            }

            glm::vec2 uv = {0.0f,0.0f};
            if(vertexUVW) {
                uv = {
                        vertexUVW[vertexIndex].x,
                        vertexUVW[vertexIndex].y,
                };
            }

            vertices.push_back({
                                       {vec.x, vec.y, vec.z},
                                       color,
                                       uv
            });
        }

        for(size_t faceIndex = 0; faceIndex < mesh->mNumFaces; faceIndex++) {
            const aiFace face = mesh->mFaces[faceIndex];
            if(face.mNumIndices != 3) {
                throw runtime_error("Can only load triangles.");
            }

            indices.push_back(face.mIndices[0]);
            indices.push_back(face.mIndices[1]);
            indices.push_back(face.mIndices[2]);
        }

        meshes[material.get()].push_back(make_shared<Mesh>(engine, vertices, indices));
    }
}

void Carrot::Model::draw(vk::CommandBuffer& commands) {
    for(const auto& material : materials) {
        // TODO: bind graphics pipeline
        // TODO: bind descriptors

        for(const auto& mesh : meshes[material.get()]) {
            mesh->bind(commands);
            mesh->draw(commands);
        }
    }
}
