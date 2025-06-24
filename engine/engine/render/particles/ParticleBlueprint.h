//
// Created by jglrxavpok on 13/05/2021.
//

#pragma once

#include <vector>
#include <iostream>
#include <memory>
#include <engine/render/resources/Pipeline.h>
#include <core/expressions/Expressions.h>
#include <core/UniquePtr.hpp>

namespace Carrot {
    struct Engine;
    struct Particle;

    class ComputePipeline;

    /// Defines how a particle inits, updates and renders itself
    class ParticleBlueprint {
    public:
        static constexpr char Magic[] = "carrot particle";

        explicit ParticleBlueprint(const std::string& filename);
        explicit ParticleBlueprint(std::vector<uint32_t>&& computeCode, std::vector<uint32_t>&& fragmentCode, bool opaque);

        friend std::ostream& operator<<(std::ostream& out, const ParticleBlueprint& blueprint);

    public:
        std::unique_ptr<ComputePipeline> buildComputePipeline(Carrot::Engine& engine, const vk::DescriptorBufferInfo particleBuffer, const vk::DescriptorBufferInfo statsBuffer) const;
        std::unique_ptr<Pipeline> buildRenderingPipeline(Carrot::Engine& engine) const;

        /// Set to true if particle file has changed, and the file could be read
        bool hasHotReloadPending();

        /// Reset the internal flag to know if hot reloading is ready. Use this after recreating the pipelines
        void clearHotReloadFlag();

    public:
        const std::vector<std::uint32_t>& getComputeShader() const { return computeShaderCode; }
        const std::vector<std::uint32_t>& getFragmentShader() const { return fragmentShaderCode; }

        bool isOpaque() const { return opaque; }

    private:
        std::uint32_t version = 0;
        std::vector<std::uint32_t> computeShaderCode;
        std::vector<std::uint32_t> fragmentShaderCode;
        bool opaque = false;

        bool readyForHotReload = false;
        Carrot::UniquePtr<Carrot::IO::FileWatcher> pFileWatcher;
    };

    std::ostream& operator<<(std::ostream& out, const ParticleBlueprint& blueprint);
}
