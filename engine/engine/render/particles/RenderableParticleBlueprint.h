//
// Created by jglrxavpok on 25/06/25.
//

#pragma once
#include <core/data/ParticleBlueprint.h>
#include <engine/render/resources/Pipeline.h>

namespace Carrot {
    class RenderableParticleBlueprint: public ParticleBlueprint {
    public:
        explicit RenderableParticleBlueprint(const Carrot::IO::Resource& file): ParticleBlueprint(file) {}
        explicit RenderableParticleBlueprint(std::vector<uint32_t>&& computeCode, std::vector<uint32_t>&& fragmentCode, bool opaque, const std::unordered_map<std::string, i32>& textureIndices)
        : ParticleBlueprint(std::move(computeCode), std::move(fragmentCode), opaque, textureIndices) {}

    public:
        std::unique_ptr<ComputePipeline> buildComputePipeline(Carrot::Engine& engine, const vk::DescriptorBufferInfo particleBuffer, const vk::DescriptorBufferInfo statsBuffer) const;
        std::unique_ptr<Pipeline> buildRenderingPipeline(Carrot::Engine& engine) const;
    };
}
