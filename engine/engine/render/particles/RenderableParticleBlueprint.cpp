//
// Created by jglrxavpok on 25/06/25.
//

#include "RenderableParticleBlueprint.h"

#include <core/data/ParticleBlueprint.h>
#include <engine/Engine.h>
#include <engine/render/ComputePipeline.h>

std::unique_ptr<Carrot::ComputePipeline> Carrot::RenderableParticleBlueprint::buildComputePipeline(Carrot::Engine& engine, const vk::DescriptorBufferInfo particleBuffer, const vk::DescriptorBufferInfo statisticsBuffer) const {
    // TODO: replace with Carrot::Pipeline
    return ComputePipelineBuilder(engine)
            .shader(Carrot::IO::Resource({(std::uint8_t*)computeShaderCode.data(), computeShaderCode.size() * sizeof(std::uint32_t)}))
            .bufferBinding(vk::DescriptorType::eStorageBuffer, 0, 0, statisticsBuffer) // TODO: don't add
            .bufferBinding(vk::DescriptorType::eStorageBuffer, 1, 0, particleBuffer)
            .bufferBinding(vk::DescriptorType::eStorageBuffer, 1, 1, statisticsBuffer)
            .build();
}

std::unique_ptr<Carrot::Pipeline> Carrot::RenderableParticleBlueprint::buildRenderingPipeline(Carrot::Engine& engine) const {
    Carrot::PipelineDescription desc{ Carrot::IO::Resource("resources/pipelines/particles.pipeline") };

    desc.type = PipelineType::Particles;
    desc.fragmentShader = Carrot::IO::Resource({(std::uint8_t*)(fragmentShaderCode.data()), fragmentShaderCode.size() * sizeof(std::uint32_t)});

    return std::make_unique<Carrot::Pipeline>(engine.getVulkanDriver(), desc);
}