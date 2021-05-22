//
// Created by jglrxavpok on 19/05/2021.
//

#pragma once

#include "engine/expressions/Expressions.h"
#include <memory>
#include <vector>
#include <SPIRV/SpvBuilder.h>

namespace Tools {
    class ParticleShaderGenerator {
    private:
        std::string projectName;

    public:
        explicit ParticleShaderGenerator(const std::string& projectName);

        std::vector<uint32_t> compileToSPIRV(const std::vector<std::shared_ptr<Carrot::Expression>>& expressions);
    };
}
