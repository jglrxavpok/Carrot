//
// Created by jglrxavpok on 13/05/2021.
//

#pragma once

#include <SPIRV/SpvBuilder.h>
#include <vector>
#include <memory>
#include <engine/expressions/Expressions.h>

namespace Carrot {
    struct Particle;

    /// Defines how a particle inits, updates and renders itself
    class ParticleBlueprint {
    private:
        std::vector<std::shared_ptr<Carrot::SetVariableExpression>> updateActions;
        std::vector<std::shared_ptr<Carrot::SetVariableExpression>> renderActions;

    public:
        void evaluateUpdateOnCPU(Particle* particle) const;

        std::vector<uint8_t> compileUpdateToSpirV() const;
        std::vector<uint8_t> compileRenderToSpirV() const;
    };
}
