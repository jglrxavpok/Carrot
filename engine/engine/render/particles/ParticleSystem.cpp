//
// Created by jglrxavpok on 27/04/2021.
//

#include "Particles.h"
#include <core/utils/RNG.h>
#include <engine/vulkan/SwapchainAware.h>
#include "imgui.h"
#include "engine/render/resources/Buffer.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/render/resources/BufferView.h"
#include "core/io/Resource.h"
#include "core/io/Logging.hpp"

#define DEBUG_PARTICLES 1

Carrot::ParticleSystem::ParticleSystem(Carrot::Engine& engine, Carrot::ParticleBlueprint& blueprint, std::uint64_t maxParticleCount):
        engine(engine), blueprint(blueprint), maxParticleCount(maxParticleCount),
        particleBuffer(engine.getResourceAllocator().allocateBuffer(
            sizeof(Particle) * maxParticleCount,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        )),
        statisticsBuffer(engine.getResourceAllocator().allocateBuffer(
                sizeof(ParticleStatistics),
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
        )) {

    particlePool.resize(maxParticleCount);

    renderingPipeline = blueprint.buildRenderingPipeline(engine);
    onSwapchainImageCountChange(engine.getSwapchainImageCount());

    updateParticlesCompute = blueprint.buildComputePipeline(engine, particleBuffer.asBufferInfo(), statisticsBuffer.asBufferInfo());

    statistics = statisticsBuffer.map<ParticleStatistics>();
}

void Carrot::ParticleSystem::onFrame(const Carrot::Render::Context& renderContext) {
#if DEBUG_PARTICLES
    if(ImGui::Begin("Debug ParticleSystem")) {
        ImGui::Text("Alive particle count: %lli", usedParticleCount);
        ImGui::Text("Alive particle count last frame: %lli", oldParticleCount);
        ImGui::Text("Particles spawned this frame: %lli", usedParticleCount-oldParticleCount);
    }
    ImGui::End();
#endif

    renderingPipeline->checkForReloadableShaders();

    if(gotUpdated) {
        engine.addFrameTask([this, renderContext]() {
            if(usedParticleCount <= 0)
                return;
            pullDataFromGPU();

            auto& camera = renderContext.viewport.getCamera();

            ZoneScopedN("Sort particles");
            // sort by distance to camera
            std::sort(&particlePool[0], &particlePool[usedParticleCount], [&](const Particle& a, const Particle& b) {
                auto toA = a.position - camera.getPosition();
                auto toB = b.position - camera.getPosition();

                return glm::dot(toA, toA) > glm::dot(toB, toB);
            });
            pushDataToGPU();
        });

        gotUpdated = false;
    }
}

void Carrot::ParticleSystem::pullDataFromGPU() {
    ZoneScoped;
    updateParticlesCompute->waitForCompletion();
    if(usedParticleCount > 0) {
        particleBuffer.getBuffer().copyTo(std::span<std::uint8_t>{reinterpret_cast<std::uint8_t*>(particlePool.data()), usedParticleCount*sizeof(Particle)}, particleBuffer.getStart());
    }
}

void Carrot::ParticleSystem::pushDataToGPU() {
    ZoneScoped;
    if(usedParticleCount > 0) {
        particleBuffer.getBuffer().stageUploadWithOffset(particleBuffer.getStart(), particlePool.data(), sizeof(Particle)*usedParticleCount);
        updateParticlesCompute->dispatch(ceil(1024.0f / usedParticleCount),1,1);
    }
}

void Carrot::ParticleSystem::tick(double deltaTime) {
    pullDataFromGPU();

    for(auto& emitter : emitters) {
        emitter->tick(deltaTime);
    }
    initNewParticles();
    updateParticles(deltaTime);

    pushDataToGPU();

    gotUpdated = true;
}

void Carrot::ParticleSystem::initNewParticles() {
    // TODO: compute shader
/*    for (size_t particleIndex = oldParticleCount; particleIndex < usedParticleCount; ++particleIndex) {
        // TODO: proper init?
    }*/
    oldParticleCount = usedParticleCount;
}

void Carrot::ParticleSystem::updateParticles(double deltaTime) {
    ZoneScoped;
    // remove dead particles
    auto end = std::remove_if(&particlePool[0], &particlePool[usedParticleCount], [](const auto& p) { return p.life < 0.0f; });
    auto count = std::distance(&particlePool[0], end);
    usedParticleCount = count;
    oldParticleCount = count;

    statistics->totalCount = usedParticleCount;
    statistics->deltaTime = deltaTime;
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

void Carrot::ParticleSystem::render(vk::RenderPass pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands) const {
    renderingPipeline->bind(pass, renderContext, commands);
    commands.draw(6 * usedParticleCount, 1, 0, 0);
}

void Carrot::ParticleSystem::renderOpaqueGBuffer(vk::RenderPass pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands) const {
    if(!isOpaque()) {
        return;
    }
    render(pass, renderContext, commands);
}

void Carrot::ParticleSystem::renderTransparentGBuffer(vk::RenderPass pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands) const {
    if(isOpaque()) {
        return;
    }
    render(pass, renderContext, commands);
}

bool Carrot::ParticleSystem::isOpaque() const {
    return blueprint.isOpaque();
}

void Carrot::ParticleSystem::onSwapchainImageCountChange(std::size_t newCount) {
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

Carrot::Pipeline& Carrot::ParticleSystem::getRenderingPipeline() {
    return *renderingPipeline;
}

Carrot::ComputePipeline& Carrot::ParticleSystem::getComputePipeline() {
    return *updateParticlesCompute;
}