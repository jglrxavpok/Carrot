//
// Created by jglrxavpok on 27/04/2021.
//

#include "Particles.h"
#include <core/utils/RNG.h>
#include <engine/Engine.h>
#include <engine/vulkan/SwapchainAware.h>
#include "imgui.h"
#include "engine/render/resources/Buffer.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/render/resources/BufferView.h"
#include "core/io/Resource.h"
#include "core/io/Logging.hpp"

#define DEBUG_PARTICLES 1

Carrot::ParticleSystem::ParticleSystem(Carrot::Engine& engine, Carrot::RenderableParticleBlueprint& blueprint, std::uint64_t maxParticleCount):
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

    emitterData = engine.getResourceAllocator().allocateStagingBuffer(sizeof(ParticleEmitter));

    renderingPipeline = blueprint.buildRenderingPipeline(engine);
    updateParticlesCompute = blueprint.buildComputePipeline(engine, particleBuffer.asBufferInfo(), statisticsBuffer.asBufferInfo());

    onSwapchainImageCountChange(engine.getSwapchainImageCount());

    statistics = statisticsBuffer.map<ParticleStatistics>();
}

void Carrot::ParticleSystem::onFrame(const Carrot::Render::Context& renderContext) {
    // clear old data
    if (!emitterDataGraveyard.empty()) {
        emitterDataGraveyard.clear();
    }

#if DEBUG_PARTICLES
    if(ImGui::Begin("Debug ParticleSystem")) {
        ImGui::Text("Alive particle count: %lli", usedParticleCount);
        ImGui::Text("Alive particle count last frame: %lli", oldParticleCount);
        ImGui::Text("Particles spawned this frame: %lli", usedParticleCount-oldParticleCount);
    }
    ImGui::End();
#endif

    renderingPipeline->checkForReloadableShaders();

    // push particle update
    if(gotUpdated) {
        engine.addFrameTask([this, renderContext]() {
            if(usedParticleCount <= 0)
                return;
            pullDataFromGPU();

            auto& camera = renderContext.pViewport->getCamera();

            ZoneScopedN("Sort particles");
            // sort by distance to camera
            const glm::vec3 cameraPos = camera.computePosition();
            std::sort(&particlePool[0], &particlePool[usedParticleCount], [&](const Particle& a, const Particle& b) {
                auto toA = a.position - cameraPos;
                auto toB = b.position - cameraPos;

                return glm::dot(toA, toA) > glm::dot(toB, toB);
            });
            pushDataToGPU();
        });

        gotUpdated = false;
    }

    // draw particles
    const Carrot::Render::PassName targetPass = isOpaque() ? Carrot::Render::PassEnum::TransparentGBuffer : Carrot::Render::PassEnum::OpaqueGBuffer;
    Render::Packet& packet = renderContext.renderer.makeRenderPacket(targetPass, Carrot::Render::PacketType::Procedural, renderContext);
    packet.pipeline = renderingPipeline;
    Render::PacketCommand& command = packet.commands.emplace_back();
    command.procedural.instanceCount = 1;
    command.procedural.vertexCount = 6 * usedParticleCount;
    renderContext.renderer.render(packet);
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
        updateParticlesCompute->dispatch(ceil(usedParticleCount / 1024.0f),1,1);
    }
}

void Carrot::ParticleSystem::tick(double deltaTime) {
    // reload if changed
    if (blueprint.hasHotReloadPending()) {
        renderingPipeline = blueprint.buildRenderingPipeline(engine);
        updateParticlesCompute = blueprint.buildComputePipeline(engine, particleBuffer.asBufferInfo(), statisticsBuffer.asBufferInfo());

        blueprint.clearHotReloadFlag();
    }

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

Carrot::EmitterData& Carrot::ParticleSystem::getEmitterData(u32 emitterID) {
    verify(emitterData.view.getSize() > sizeof(EmitterData) * emitterID, "Emitter ID outside of emitterData!");
    return emitterData.view.map<EmitterData>()[emitterID];
}

std::shared_ptr<Carrot::ParticleEmitter> Carrot::ParticleSystem::createEmitter() {
    u32 index = nextEmitterID++;
    emitters.emplaceBack(std::make_shared<ParticleEmitter>(*this, index));

    // allocate new buffer and copy data if the current buffer is too small
    const u32 emitterCountOnGPU = emitterData.view.getSize() / sizeof(EmitterData);
    if (index >= emitterCountOnGPU) {
        const u32 nextCount = emitterCountOnGPU*2;
        BufferAllocation newBuffer = GetResourceAllocator().allocateStagingBuffer(sizeof(EmitterData) * nextCount);
        GetRenderer().queueAsyncCopy(false, emitterData.view, newBuffer.view);

        // old 'emitterData' must stay alive for the next frame (for the async copy to be done)
        emitterDataGraveyard.emplaceBack(std::move(emitterData));
        emitterData = std::move(newBuffer);

        for (int i = 0; i < engine.getSwapchainImageCount(); ++i) {
            auto set = renderingPipeline->getDescriptorSets(GetEngine().newRenderContext(i, GetEngine().getMainViewport()), 1)[i];

            const vk::DescriptorBufferInfo emitterBufferInfo = emitterData.view.asBufferInfo();
            vk::WriteDescriptorSet writeEmitters = {
                .dstSet = set,
                .dstBinding = 2,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &emitterBufferInfo,
            };

            engine.getLogicalDevice().updateDescriptorSets({writeEmitters}, {});
        }
    }

    return emitters.back();
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
        auto set = renderingPipeline->getDescriptorSets(GetEngine().newRenderContext(i, GetEngine().getMainViewport()), 1)[i];

        const vk::DescriptorBufferInfo particleBufferInfo = particleBuffer.asBufferInfo();
        vk::WriteDescriptorSet writeParticles = {
                .dstSet = set,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &particleBufferInfo,
        };

        const vk::DescriptorBufferInfo emitterBufferInfo = emitterData.view.asBufferInfo();
        vk::WriteDescriptorSet writeEmitters = {
            .dstSet = set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo = &emitterBufferInfo,
        };

        engine.getLogicalDevice().updateDescriptorSets({writeParticles, writeEmitters}, {});
    }
}

void Carrot::ParticleSystem::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) {}

Carrot::Pipeline& Carrot::ParticleSystem::getRenderingPipeline() {
    return *renderingPipeline;
}

Carrot::ComputePipeline& Carrot::ParticleSystem::getComputePipeline() {
    return *updateParticlesCompute;
}