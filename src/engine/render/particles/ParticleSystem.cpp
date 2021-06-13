//
// Created by jglrxavpok on 27/04/2021.
//

#include "Particles.h"
#include <engine/utils/RNG.h>
#include <engine/vulkan/SwapchainAware.h>
#include "imgui.h"
#include "engine/render/resources/Buffer.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/render/resources/BufferView.h"
#include "engine/io/Resource.h"

#define DEBUG_PARTICLES 1

Carrot::ParticleSystem::ParticleSystem(Carrot::Engine& engine, Carrot::ParticleBlueprint& blueprint, std::uint64_t maxParticleCount):
        engine(engine), blueprint(blueprint), maxParticleCount(maxParticleCount),
        particleBuffer(engine.getResourceAllocator().allocateBuffer(
            sizeof(Particle) * maxParticleCount,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
        )),
        drawCommandBuffer(engine.getResourceAllocator().allocateBuffer(
                sizeof(vk::DrawIndirectCommand),
                vk::BufferUsageFlagBits::eIndirectBuffer,
                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
        )),
        statisticsBuffer(engine.getResourceAllocator().allocateBuffer(
                sizeof(ParticleStatistics),
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
        )) {

    drawCommand = drawCommandBuffer.map<vk::DrawIndirectCommand>();
    drawCommand->firstInstance = 0;
    drawCommand->firstVertex = 0;
    drawCommand->instanceCount = 1;
    drawCommand->vertexCount = 0;

    particlePool = particleBuffer.map<Particle>();
    particlePoolEnd = particlePool + maxParticleCount;

    renderingPipeline = engine.getRenderer().getOrCreatePipeline("particles");
    onSwapchainImageCountChange(engine.getSwapchainImageCount());

    std::fill(particlePool, particlePoolEnd, Particle{});

    updateParticlesCompute = blueprint.buildComputePipeline(engine, particleBuffer.asBufferInfo(), statisticsBuffer.asBufferInfo());

    statistics = statisticsBuffer.map<ParticleStatistics>();
}

void Carrot::ParticleSystem::onFrame(size_t frameIndex) {
    drawCommand->vertexCount = usedParticleCount*6;

#if DEBUG_PARTICLES
    if(ImGui::Begin("Debug ParticleSystem")) {
        ImGui::Text("Alive particle count: %lli", usedParticleCount);
        ImGui::Text("Alive particle count last frame: %lli", oldParticleCount);
        ImGui::Text("Particles spawned this frame: %lli", usedParticleCount-oldParticleCount);
    }
    ImGui::End();
#endif
}

void Carrot::ParticleSystem::tick(double deltaTime) {
    for(auto& emitter : emitters) {
        emitter->tick(deltaTime);
    }

    initNewParticles();
    updateParticles(deltaTime);
}

void Carrot::ParticleSystem::initNewParticles() {
    // TODO: compute shader
/*    for (size_t particleIndex = oldParticleCount; particleIndex < usedParticleCount; ++particleIndex) {
        // TODO: proper init?
    }*/
    oldParticleCount = usedParticleCount;
}

void Carrot::ParticleSystem::updateParticles(double deltaTime) {
    updateParticlesCompute->waitForCompletion();

    // remove dead particles
    auto end = std::remove_if(particlePool, particlePool+usedParticleCount, [](const auto& p) { return p.life < 0.0f; });
    auto count = std::distance(particlePool, end);
    usedParticleCount = count;
    oldParticleCount = count;

    statistics->totalCount = usedParticleCount;
    statistics->deltaTime = deltaTime;
    updateParticlesCompute->dispatch(ceil(usedParticleCount/1024.0f),1,1);
}

std::shared_ptr<Carrot::ParticleEmitter> Carrot::ParticleSystem::createEmitter() {
    emitters.emplace_back(std::make_shared<ParticleEmitter>(*this));
    return emitters[emitters.size()-1];
}

Carrot::Particle* Carrot::ParticleSystem::getFreeParticle() {
    if(usedParticleCount >= maxParticleCount)
        return nullptr;
    auto* particle = &particlePool[usedParticleCount++];
    *particle = {};
    return particle;
}

void Carrot::ParticleSystem::gBufferRender(size_t frameIndex, vk::CommandBuffer& commands) const {
    renderingPipeline->bind(frameIndex, commands);
    commands.drawIndirect(drawCommandBuffer.getVulkanBuffer(), drawCommandBuffer.getStart(), 1, sizeof(vk::DrawIndexedIndirectCommand));
}

void Carrot::ParticleSystem::onSwapchainImageCountChange(size_t newCount) {
    renderingPipeline->onSwapchainImageCountChange(newCount);
    for (int i = 0; i < engine.getSwapchainImageCount(); ++i) {
        auto set = renderingPipeline->getDescriptorSets0()[i];

        vk::DescriptorBufferInfo bufferInfo {
            .buffer = particleBuffer.getVulkanBuffer(),
            .offset = particleBuffer.getStart(),
            .range = particleBuffer.getSize(),
        };
        vk::WriteDescriptorSet write = {
                .dstSet = set,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBufferDynamic,
                .pBufferInfo = &bufferInfo,
        };

        engine.getLogicalDevice().updateDescriptorSets(write, {});
    }
}

void Carrot::ParticleSystem::onSwapchainSizeChange(int newWidth, int newHeight) {}
