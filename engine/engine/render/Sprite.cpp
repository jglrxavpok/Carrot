//
// Created by jglrxavpok on 28/07/2021.
//

#include "Sprite.h"
#include <utility>
#include <engine/render/resources/Vertex.h>
#include "engine/render/resources/Mesh.h"
#include "engine/render/resources/ResourceAllocator.h"
#include <glm/gtx/quaternion.hpp>

namespace Carrot::Render {

    std::unique_ptr<Carrot::Mesh> Sprite::spriteMesh = nullptr;

    Sprite::Sprite(Carrot::VulkanRenderer& renderer, Carrot::Render::Texture::Ref texture, Carrot::Math::Rect2Df textureRegion): renderer(renderer), textureRegion(std::move(textureRegion)),
    instanceBuffer(renderer.getEngine().getResourceAllocator().allocateBuffer(sizeof(Carrot::InstanceData), vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)) {
/*        Carrot::PipelineDescription desc = Carrot::PipelineDescription("resources/pipelines/gBufferSprite.json");
        renderingPipeline = std::make_unique<Carrot::Pipeline>(renderer.getVulkanDriver(), desc);*/
        // create a different pipeline for each texture (probably not really efficient)
        setTexture(texture);
        instanceData = instanceBuffer.map<Carrot::InstanceData>();
        *instanceData = Carrot::InstanceData();
    }

    void Sprite::onFrame(Carrot::Render::Context renderContext) const {
        ZoneScoped;
        {
            ZoneScopedN("Instance update");
            instanceData->transform = computeTransformMatrix();
            instanceData->color = {1.0, 1.0, 1.0, 1.0};
        }
        {
            ZoneScopedN("Bind texture");
            renderer.bindTexture(*renderingPipeline, renderContext, *texture, 0, 0, nullptr);
        }
        {
            ZoneScopedN("Bind sampler");
            renderer.bindSampler(*renderingPipeline, renderContext, renderer.getVulkanDriver().getNearestSampler(), 0, 1);
        }
    }

    void Sprite::soloGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) const {
        ZoneScoped;
        auto& mesh = getSpriteMesh(renderContext.renderer.getEngine());
        renderContext.renderer.pushConstantBlock("region", *renderingPipeline, renderContext, vk::ShaderStageFlagBits::eVertex, commands, textureRegion);
        renderingPipeline->bind(renderPass, renderContext, commands);
        mesh.bind(commands);
        commands.bindVertexBuffers(1, instanceBuffer.getVulkanBuffer(), instanceBuffer.getStart());
        mesh.draw(commands);
    }

    void Sprite::updateTextureRegion(const Carrot::Math::Rect2Df& newRegion) {
        textureRegion = newRegion;
    }

    Carrot::Mesh& Sprite::getSpriteMesh(Carrot::Engine& engine) {
        if(!spriteMesh) {
            spriteMesh = make_unique<Carrot::Mesh>(engine.getVulkanDriver(),
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
        auto clone = std::make_shared<Sprite>(renderer, texture, textureRegion);
        *clone->instanceData = *instanceData;
        clone->size = size;
        clone->position = position;
        clone->rotation = rotation;
        clone->parentTransform = parentTransform;
        return clone;
    }

    void Sprite::setTexture(Texture::Ref texture) {
        this->texture = std::move(texture);
        verify(this->texture != nullptr, "Cannot create sprite with no texture");
        renderingPipeline = renderer.getOrCreatePipeline("gBufferSprite", (std::uint64_t)((VkImage)this->texture->getVulkanImage()));
    }
}