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
#include "ParticleBlueprint.h"

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
    public:
        explicit ParticleSystem(Carrot::Engine& engine, ParticleBlueprint& blueprint, std::uint64_t maxParticleCount);

        std::shared_ptr<ParticleEmitter> createEmitter();

        /// Gets a free particle inside the particle pool. Do not keep this pointer. Returns nullptr if none is available
        /// Not thread-safe
        Particle* getFreeParticle();

        Pipeline& getRenderingPipeline();
        ComputePipeline& getComputePipeline();

        void tick(double deltaTime);

        void onFrame(const Carrot::Render::Context& renderContext);

        void gBufferRender(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) const;

        void onSwapchainImageCountChange(std::size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    private:
        void pullDataFromGPU();
        void pushDataToGPU();

    private:
        ParticleBlueprint& blueprint;
        Carrot::Engine& engine;

        /// All particles will be kept inside this buffer, sorted by lifetime each frame
        ///  oldParticleCount will keep the number of particles active last frame, while
        ///  usedParticleCount will keep the number of all active particles.
        ///  Before a call to initNewParticles, usedParticleCount-oldParticleCount represent the newly created particle during the frame
        Carrot::BufferView particleBuffer;
        std::vector<Particle> particlePool;
        std::size_t oldParticleCount = 0;
        std::size_t usedParticleCount = 0;

        std::vector<std::shared_ptr<ParticleEmitter>> emitters;
        std::uint64_t maxParticleCount;

        std::shared_ptr<Pipeline> renderingPipeline = nullptr;
        std::shared_ptr<ComputePipeline> updateParticlesCompute = nullptr;

        Carrot::BufferView statisticsBuffer;
        ParticleStatistics* statistics = nullptr;
        std::atomic<bool> gotUpdated = false;

        void initNewParticles();
        void updateParticles(double deltaTime);
    };
}
