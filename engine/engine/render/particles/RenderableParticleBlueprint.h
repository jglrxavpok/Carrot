//
// Created by jglrxavpok on 25/06/25.
//

#pragma once
#include <core/data/ParticleBlueprint.h>
#include <engine/render/resources/Pipeline.h>

namespace Carrot {
    class RenderableParticleBlueprint: public ParticleBlueprint {
    public:
        // TODO: use Carrot::IO::Resource
        explicit RenderableParticleBlueprint(const Carrot::IO::Resource& file): ParticleBlueprint(file) {}
        explicit RenderableParticleBlueprint(std::vector<uint32_t>&& computeCode, std::vector<uint32_t>&& fragmentCode, bool opaque)
        : ParticleBlueprint(std::move(computeCode), std::move(fragmentCode), opaque) {}

    public:
        std::unique_ptr<ComputePipeline> buildComputePipeline(Carrot::Engine& engine, const vk::DescriptorBufferInfo particleBuffer, const vk::DescriptorBufferInfo statsBuffer) const;
        std::unique_ptr<Pipeline> buildRenderingPipeline(Carrot::Engine& engine) const;
    };
}
