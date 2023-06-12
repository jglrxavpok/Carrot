//
// Created by jglrxavpok on 19/01/2022.
//

#include "Colliders.h"
#include <reactphysics3d/reactphysics3d.h>
#include "engine/utils/Macros.h"
#include "engine/render/resources/Mesh.h"
#include "engine/render/Model.h"
#include "engine/utils/conversions.h"
#include "PhysicsSystem.h"

namespace Carrot::Physics {
    StaticConcaveMeshCollisionShape::StaticConcaveMeshCollisionShape() {
        triangleMesh = GetPhysics().getCommons().createTriangleMesh();
        shape = GetPhysics().getCommons().createConcaveMeshShape(triangleMesh);
    }

    StaticConcaveMeshCollisionShape::StaticConcaveMeshCollisionShape(const rapidjson::Value& json) /* don't call default constructor, we need shape = nullptr */ {
        setModel(GetRenderer().getOrCreateModel(json["model_path"].GetString()));
    }

    StaticConcaveMeshCollisionShape::StaticConcaveMeshCollisionShape(const std::shared_ptr<Carrot::Model>& model) /* don't call default constructor, we need shape = nullptr */ {
        setModel(model);
    }

    void StaticConcaveMeshCollisionShape::setModel(const std::shared_ptr<Carrot::Model>& model) {
        this->gpuModel = model;

        // delete later, so that reattaching works
        reactphysics3d::ConcaveMeshShape* oldShape = shape;
        reactphysics3d::TriangleMesh* oldMesh = triangleMesh;
        meshes.clear();
        triangleMesh = GetPhysics().getCommons().createTriangleMesh();

        for(auto& gpuMesh : model->getStaticMeshes()) {
            meshes.emplace_back();
            auto& cpuMesh = meshes.back();
            verify(gpuMesh->getVertexBufferSize() / gpuMesh->getVertexCount() == sizeof(Carrot::Vertex), "assumes VertexFormat::Vertex");

            std::vector<Carrot::Vertex> gpuData;
            gpuData.resize(gpuMesh->getVertexCount());
            cpuMesh.positions.resize(gpuMesh->getVertexCount());
            cpuMesh.normals.resize(gpuMesh->getVertexCount());
            cpuMesh.indices.resize(gpuMesh->getIndexCount());

            gpuMesh->getVertexBuffer().download(std::span<std::uint8_t>(reinterpret_cast<std::uint8_t *>(gpuData.data()), gpuData.size() * sizeof(Carrot::Vertex)));

            for (std::size_t i = 0; i < gpuData.size(); ++i) {
                const auto& vertex = gpuData[i];
                cpuMesh.positions[i] = {vertex.pos.x, vertex.pos.y, vertex.pos.z};
                cpuMesh.normals[i] = vertex.normal;
            }

            gpuMesh->getIndexBuffer().download(std::span<std::uint8_t>(reinterpret_cast<std::uint8_t *>(cpuMesh.indices.data()), cpuMesh.indices.size() * sizeof(std::uint32_t)));

            // TODO: parallelize
            // inverse winding order for front face detection. Reactphysics wants counter-clockwise
            for(std::size_t triangleIndex = 0; triangleIndex < cpuMesh.indices.size() / 3; triangleIndex++) {
                std::swap(cpuMesh.indices[triangleIndex * 3 + 0],
                          cpuMesh.indices[triangleIndex * 3 + 2]);
            }
        }

        for(auto& cpuMesh : meshes) {
            triangleMesh->addSubpart(cpuMesh.makeReactphysicsVertexArray().get());
        }
        shape = GetPhysics().getCommons().createConcaveMeshShape(triangleMesh);

        if(oldShape || oldMesh) {
            verify(oldShape && oldMesh, "Both the shape and the mesh should exist at the same time");
            reattachCollider();
            GetPhysics().getCommons().destroyTriangleMesh(oldMesh);
            GetPhysics().getCommons().destroyConcaveMeshShape(oldShape);
        }
    }

    std::unique_ptr<reactphysics3d::TriangleVertexArray>& StaticConcaveMeshCollisionShape::BasicCPUMesh::makeReactphysicsVertexArray() {
        vertexArray = std::make_unique<reactphysics3d::TriangleVertexArray>(positions.size(),
                                                                            positions.data(),
                                                                            sizeof(glm::vec3),
                                                                            normals.data(),
                                                                            sizeof(glm::vec3),
                                                                            indices.size() / 3,
                                                                            indices.data(),
                                                                            sizeof(std::uint32_t) * 3,
                                                                            reactphysics3d::TriangleVertexArray::VertexDataType::VERTEX_FLOAT_TYPE,
                                                                            reactphysics3d::TriangleVertexArray::NormalDataType::NORMAL_FLOAT_TYPE,
                                                                            reactphysics3d::TriangleVertexArray::IndexDataType::INDEX_INTEGER_TYPE
        );
        return vertexArray;
    }

    Carrot::Model& StaticConcaveMeshCollisionShape::getModel() const {
        return *gpuModel;
    }

    void StaticConcaveMeshCollisionShape::fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const {
        object.AddMember("model_path", rapidjson::Value(gpuModel->getOriginatingResource().getName().c_str(), allocator), allocator);
    }

    void StaticConcaveMeshCollisionShape::setScale(const glm::vec3& scaling) {
        verify(scaling.x <= 1.0f && scaling.y <= 1.0f && scaling.z <= 1.0f, "ConcaveMeshShape has collision issue with scale > 1.0. See issue https://github.com/DanielChappuis/reactphysics3d/issues/206");
        shape->setScale(Carrot::reactPhysicsVecFromGlm(scaling));
    }

    glm::vec3 StaticConcaveMeshCollisionShape::getScale() const {
        return Carrot::glmVecFromReactPhysics(shape->getScale());
    }

    StaticConcaveMeshCollisionShape::~StaticConcaveMeshCollisionShape() {
        GetPhysics().getCommons().destroyConcaveMeshShape(shape);
        GetPhysics().getCommons().destroyTriangleMesh(triangleMesh);
        meshes.clear();
    }
}