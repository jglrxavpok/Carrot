//
// Created by jglrxavpok on 13/05/2021.
//

#pragma once

#include <SPIRV/SpvBuilder.h>
#include <vector>
#include <iostream>
#include <memory>
#include <engine/expressions/Expressions.h>

namespace Carrot {
    struct Particle;

    /// Defines how a particle inits, updates and renders itself
    class ParticleBlueprint {
    public:
        static constexpr char Magic[] = "carrot particle";

        explicit ParticleBlueprint(const std::string& filename);
        explicit ParticleBlueprint(std::vector<uint32_t>&& computeCode, std::vector<uint32_t>&& fragmentCode);

        friend std::ostream& operator<<(std::ostream& out, const ParticleBlueprint& blueprint);
    private:
        std::uint32_t version = 0;
        std::vector<std::uint32_t> computeShaderCode;
        std::vector<std::uint32_t> fragmentShaderCode;
    };

    std::ostream& operator<<(std::ostream& out, const ParticleBlueprint& blueprint);
}
