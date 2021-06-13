//
// Created by jglrxavpok on 13/05/2021.
//

#pragma once

#include <SPIRV/SpvBuilder.h>
#include <vector>
#include <iostream>
#include <memory>
#include <engine/expressions/Expressions.h>
#include "engine/vulkan/includes.h"
#include "engine/Engine.h"

namespace Carrot {
    struct Particle;

    class ComputePipeline;

    /// Defines how a particle inits, updates and renders itself
    class ParticleBlueprint {
    public:
        static constexpr char Magic[] = "carrot particle";

        explicit ParticleBlueprint(const std::string& filename);
        explicit ParticleBlueprint(std::vector<uint32_t>&& computeCode, std::vector<uint32_t>&& fragmentCode);

        friend std::ostream& operator<<(std::ostream& out, const ParticleBlueprint& blueprint);

    public:
        std::unique_ptr<ComputePipeline> buildComputePipeline(Carrot::Engine& engine, const vk::DescriptorBufferInfo particleBuffer, const vk::DescriptorBufferInfo statsBuffer);

    public:
        const std::vector<std::uint32_t>& getComputeShader() const { return computeShaderCode; }
        const std::vector<std::uint32_t>& getFragmentShader() const { return fragmentShaderCode; }

    private:
        std::uint32_t version = 0;
        std::vector<std::uint32_t> computeShaderCode;
        std::vector<std::uint32_t> fragmentShaderCode;
    };

    std::ostream& operator<<(std::ostream& out, const ParticleBlueprint& blueprint);
}
