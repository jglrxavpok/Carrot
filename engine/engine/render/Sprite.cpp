//
// Created by jglrxavpok on 28/07/2021.
//

#include "Sprite.h"
#include <utility>
#include <engine/render/resources/Vertex.h>
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/render/resources/SingleMesh.h"
#include "engine/render/DrawData.h"
#include <glm/gtx/quaternion.hpp>

namespace Carrot::Render {

    std::unique_ptr<Carrot::Mesh> Sprite::spriteMesh = nullptr;

    Sprite::Sprite() {
        renderingPipeline = GetRenderer().getOrCreatePipeline("gBufferSprite");
    }

    Sprite::Sprite(Carrot::Render::Texture::Ref texture, Carrot::Math::Rect2Df textureRegion): textureRegion(std::move(textureRegion)) {
        renderingPipeline = GetRenderer().getOrCreatePipeline("gBufferSprite");
        setTexture(texture);
    }

    void Sprite::onFrame(const Carrot::Render::Context& renderContext) const {
        if(!texture)
            return;
        ZoneScoped;
        Carrot::InstanceData instanceData;
        {
            ZoneScopedN("Instance update");
            instanceData.transform = computeTransformMatrix();
            instanceData.color = color;
        }

        Render::Packet packet = renderContext.renderer.makeRenderPacket(Render::PassEnum::OpaqueGBuffer, renderContext.viewport);
        packet.useMesh(getSpriteMesh(GetEngine()));
        packet.viewport = &renderContext.viewport;
        packet.pipeline = renderingPipeline;

        packet.useInstance(instanceData);

        Render::Packet::PushConstant& materialData = packet.addPushConstant();
        materialData.id = "drawDataPush";
        materialData.stages = vk::ShaderStageFlagBits::eFragment;

        Carrot::DrawData drawData;
        drawData.materialIndex = material->getSlot();
        materialData.setData(drawData);

        Render::Packet::PushConstant& region = packet.addPushConstant();
        region.id = "region";
        region.stages = vk::ShaderStageFlagBits::eVertex;
        region.setData(textureRegion);

        renderContext.renderer.render(packet);
    }

    void Sprite::updateTextureRegion(const Carrot::Math::Rect2Df& newRegion) {
        textureRegion = newRegion;
    }

    Carrot::Mesh& Sprite::getSpriteMesh(Carrot::Engine& engine) {
        if(!spriteMesh) {
            spriteMesh = make_unique<Carrot::SingleMesh>(
                                                   std::vector<Carrot::SimpleVertexWithInstanceData>{
                                                           { { -0.5f, -0.5f, 0.0f } },
                                                           { { 0.5f, -0.5f, 0.0f } },
                                                           { { 0.5f, 0.5f, 0.0f } },
                                                           { { -0.5f, 0.5f, 0.0f } },
                                                   },
                                                   std::vector<uint32_t>{
                                                           2,1,0,
                                                           3,2,0,
                                                   });
        }
        return *spriteMesh;
    }

    glm::mat4 Sprite::computeTransformMatrix() const {
        return parentTransform * glm::translate(glm::mat4(1.0f), position) * glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), glm::vec3(size.x, size.y, 1.0));
    }

    void Sprite::cleanup() {
        spriteMesh.reset();
    }

    std::shared_ptr<Sprite> Sprite::duplicate() const {
        auto clone = std::make_shared<Sprite>(texture, textureRegion);
        clone->size = size;
        clone->position = position;
        clone->rotation = rotation;
        clone->parentTransform = parentTransform;
        clone->material = material;
        return clone;
    }

    void Sprite::setTexture(Texture::Ref texture) {
        this->texture = texture;
        verify(this->texture != nullptr, "Cannot create sprite with no texture");
        material = GetRenderer().getMaterialSystem().createMaterialHandle();
        material->albedo = GetRenderer().getMaterialSystem().createTextureHandle(texture);
    }
}