//
// Created by jglrxavpok on 20/02/2022.
//

#include "CameraRenderer.h"
#include <engine/render/GBufferDrawData.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/assets/AssetServer.h>

namespace Peeler::ECS {
    CameraRenderer::CameraRenderer(Carrot::ECS::World& world): Carrot::ECS::RenderSystem<Carrot::ECS::TransformComponent, Carrot::ECS::CameraComponent>(world) {
        cameraModel = Carrot::AsyncModelResource(GetAssetServer().loadModelTask("resources/models/camera.gltf"));
        primaryCameraPipeline = GetRenderer().getOrCreatePipeline("gBuffer");
        secondaryCameraPipeline = GetRenderer().getOrCreatePipeline("gBufferWireframe");
    }

    void CameraRenderer::onFrame(const Carrot::Render::Context& renderContext) {
        if(!cameraModel.isReady()) {
            return;
        }
        Carrot::Render::Packet& packet = GetRenderer().makeRenderPacket(Carrot::Render::PassEnum::OpaqueGBuffer, Carrot::Render::PacketType::DrawIndexedInstanced, renderContext);
        packet.useMesh(*cameraModel->getStaticMeshes()[0]);

        static glm::mat4 scaling = glm::scale(glm::mat4{ 1.0f }, glm::vec3 { 0.1f, 0.1f, 0.1f });
        static glm::mat4 localRotate = glm::rotate(glm::mat4{ 1.0f }, glm::pi<float>()/2.0f, glm::vec3 { 1.0f, 0.f, 0.f });

        Carrot::InstanceData instanceData;
        Carrot::GBufferDrawData data;

        data.materialIndex = GetRenderer().getWhiteMaterial().getSlot();
        packet.addPerDrawData({&data, 1});

        forEachEntity([&](Carrot::ECS::Entity& entity, Carrot::ECS::TransformComponent& transform, Carrot::ECS::CameraComponent& cameraComponent) {
            instanceData.uuid = entity.getID();
            instanceData.transform = transform.toTransformMatrix() * scaling * localRotate;
            instanceData.lastFrameTransform = transform.lastFrameGlobalTransform * scaling;
            packet.useInstance(instanceData);

            packet.pipeline = cameraComponent.isPrimary ? primaryCameraPipeline : secondaryCameraPipeline;

            GetRenderer().render(packet);
        });
    }

    std::unique_ptr<Carrot::ECS::System> CameraRenderer::duplicate(Carrot::ECS::World& newOwner) const {
        return std::make_unique<CameraRenderer>(newOwner);
    }
}