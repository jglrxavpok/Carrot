//
// Created by jglrxavpok on 19/05/2021.
//

#pragma once

#include "core/expressions/Expressions.h"
#include <memory>
#include <vector>
#include <SPIRV/SpvBuilder.h>

namespace Fertilizer {
    enum class ParticleShaderMode {
        Fragment,
        ComputeUpdateParticle,
    };

    struct ParticleShadersMetadata {
        /**
         * Particle shaders have access to an array of textures. This map gives the mapping between a texture name and
         * its slot inside the array
         */
        std::unordered_map<std::string, i32> imageIndices;
    };

    struct CompiledParticleShaders {
        std::vector<u32> fragment;
        std::vector<u32> computeUpdate;
    };

    struct ParticleShaderInputs {
        std::vector<std::shared_ptr<Carrot::Expression>> fragment;
        std::vector<std::shared_ptr<Carrot::Expression>> computeUpdate;
    };

    class ParticleShaderGenerator {
    public:
        explicit ParticleShaderGenerator(const std::string& projectName);

        CompiledParticleShaders compileToSPIRV(const ParticleShaderInputs& inputs);

        const ParticleShadersMetadata& getMetadata() const;

    private:
        ParticleShadersMetadata metadata;
        std::string projectName;
        spv::Id particleStats;
        spv::Id texturesArray;
        spv::Id linearSampler;
        spv::Id nearestSampler;

        friend class SPIRVisitor;
        friend class SlangVisitor;
    };
}
