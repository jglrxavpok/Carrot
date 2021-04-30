//
// Created by jglrxavpok on 27/04/2021.
//

#pragma once

#include <engine/Engine.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <engine/vulkan/includes.h>
#include <engine/render/resources/BufferView.h>
#include <engine/render/ComputePipeline.h>

namespace Carrot {
    class ParticleSystem;
    class ParticleEmitter;
    class ParticleBlueprint;
    struct Particle;

    struct Particle {
        alignas(16) glm::vec3 position{0.0f};
        float life = -1.0f;

        alignas(16) glm::vec3 velocity{0.0f};
        float size = 1.0f;

        alignas(16) std::uint32_t id = 0;

        static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions();
        static std::vector<vk::VertexInputBindingDescription> getBindingDescription();
    };

    struct ParticleStatistics {
        float deltaTime;
        uint32_t totalCount;
    };

    /// Defines how a particle inits, updates and renders itself
    class ParticleBlueprint {
        // TODO
    };

    class ParticleEmitter {
    private:
        ParticleSystem& system;
        glm::vec3 position{0.0f};
        float rate = 1.0f; //! Particles per second

        float rateError = 0.0f;
        std::uint64_t spawnedParticles = 0;
        float time = 0.0f;

    public:
        explicit ParticleEmitter(ParticleSystem& system);

        float getRate() const {
            return rate;
        }

        void setRate(float rate) {
            this->rate = rate;
        }

        glm::vec3& getPosition() {
            return position;
        }

        const glm::vec3& getPosition() const {
            return position;
        }

        void tick(double deltaTime);
    };

    class ParticleSystem: public SwapchainAware {
    private:
        ParticleBlueprint& blueprint;
        Carrot::Engine& engine;

        /// All particles will be kept inside this buffer, sorted by lifetime each frame
        ///  oldParticleCount will keep the number of particles active last frame, while
        ///  usedParticleCount will keep the number of all active particles.
        ///  Before a call to initNewParticles, usedParticleCount-oldParticleCount represent the newly created particle during the frame
        Carrot::BufferView particleBuffer;
        Particle* particlePool = nullptr;
        Particle* particlePoolEnd = nullptr;
        size_t oldParticleCount = 0;
        size_t usedParticleCount = 0;

        std::vector<std::shared_ptr<ParticleEmitter>> emitters;
        std::uint64_t maxParticleCount;

        shared_ptr<Pipeline> renderingPipeline = nullptr;
        shared_ptr<ComputePipeline> updateParticlesCompute = nullptr;

        Carrot::BufferView drawCommandBuffer;
        vk::DrawIndirectCommand* drawCommand = nullptr;
        Carrot::BufferView statisticsBuffer;
        ParticleStatistics* statistics = nullptr;

        void initNewParticles();
        void updateParticles(double deltaTime);

    public:
        explicit ParticleSystem(Carrot::Engine& engine, ParticleBlueprint& blueprint, std::uint64_t maxParticleCount);

        std::shared_ptr<ParticleEmitter> createEmitter();

        /// Gets a free particle inside the particle pool. Do not keep this pointer. Returns nullptr if none is available
        /// Not thread-safe
        Particle* getFreeParticle();

        void tick(double deltaTime);

        void onFrame(size_t frameIndex);

        void gBufferRender(size_t frameIndex, vk::CommandBuffer& commands) const;

        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;
    };
}