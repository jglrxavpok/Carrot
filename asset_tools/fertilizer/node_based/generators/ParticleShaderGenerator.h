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

    class ParticleShaderGenerator {
    public:
        explicit ParticleShaderGenerator(const std::string& projectName);

        std::vector<uint32_t> compileToSPIRV(ParticleShaderMode shaderMode, const std::vector<std::shared_ptr<Carrot::Expression>>& expressions);

        const ParticleShadersMetadata& getMetadata() const;

    private:
        ParticleShadersMetadata metadata;
        std::string projectName;
        spv::Id particleStats;
        spv::Id texturesArray;
        spv::Id linearSampler;
        spv::Id nearestSampler;

        friend class SPIRVisitor;

        void generateComputeUpdateParticle(spv::Builder& builder, spv::Id glslImport, spv::Id descriptorSet, const std::vector<std::shared_ptr<Carrot::Expression>>& expressions);
        void generateFragment(spv::Builder& builder, spv::Id glslImport, spv::Id descriptorSet, const std::vector<std::shared_ptr<Carrot::Expression>>& expressions);
    };
}
