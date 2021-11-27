//
// Created by jglrxavpok on 19/05/2021.
//

#pragma once

#include "core/expressions/Expressions.h"
#include <memory>
#include <vector>
#include <SPIRV/SpvBuilder.h>

namespace Tools {
    enum class ParticleShaderMode {
        Fragment,
        Compute,
    };

    class ParticleShaderGenerator {
    private:
        std::string projectName;
        ParticleShaderMode shaderMode;

    public:
        explicit ParticleShaderGenerator(ParticleShaderMode shaderMode, const std::string& projectName);

        std::vector<uint32_t> compileToSPIRV(const std::vector<std::shared_ptr<Carrot::Expression>>& expressions);

    private:
        void generateCompute(spv::Builder& builder, spv::Id glslImport, spv::Id descriptorSet, const std::vector<std::shared_ptr<Carrot::Expression>>& expressions);
        void generateFragment(spv::Builder& builder, spv::Id glslImport, spv::Id descriptorSet, const std::vector<std::shared_ptr<Carrot::Expression>>& expressions);
    };
}
