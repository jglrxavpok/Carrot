//
// Created by jglrxavpok on 27/04/2021.
//

#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <core/allocators/TrackingAllocator.h>
#include <engine/render/resources/BufferView.h>
#include <engine/render/ComputePipeline.h>
#include <engine/render/resources/BufferAllocation.h>

#include "RenderableParticleBlueprint.h"
#include "engine/render/BasicRenderable.h"
#include "engine/render/resources/Pipeline.h"

namespace Carrot {
    class ParticleSystem;
    class ParticleEmitter;
    class ParticleBlueprint;
    struct Particle;

    struct Particle {
        glm::vec3 position{0.0f};
        float life = -1.0f;

        glm::vec3 velocity{0.0f};
        float size = 1.0f;

        std::uint32_t id = 0;
        std::uint32_t emitterID = 0;

        static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions();
        static std::vector<vk::VertexInputBindingDescription> getBindingDescription();
    };

    struct EmitterData {
        glm::mat4 emitterTransform;
    };

    struct ParticleStatistics {
        float deltaTime;
        uint32_t totalCount;
    };

    class ParticleEmitter {
    public:
        explicit ParticleEmitter(ParticleSystem& system, u32 emitterID);

        float getRate() const {
            return rate;
        }

        void setRate(float rate) {
            this->rate = rate;
        }

        const glm::vec3& getPosition() const {
            return position;
        }

        void setPosition(const glm::vec3& newPosition);

        void setRotation(const glm::quat& rotation);
        const glm::quat& getRotation() const;

        bool isEmittingInWorldSpace() const;
        void setEmittingInWorldSpace(bool inWorldSpace);

        void tick(double deltaTime);

    private:
        ParticleSystem& system;
        glm::vec3 position{0.0f};
        glm::quat rotation = glm::identity<glm::quat>();

        bool needEmitterTransformUpdate = true;
        glm::mat4 emitterTransform; // result of position+rotation, sent to particles

        float rate = 1.0f; //! Particles per second

        float rateError = 0.0f;
        std::uint64_t spawnedParticles = 0;
        float time = 0.0f;
        bool emitInWorldSpace = false;
        u32 emitterID = 0;

    };

    class ParticleSystem: public SwapchainAware, public Render::BasicRenderable {
    public:
        explicit ParticleSystem(Carrot::Engine& engine, RenderableParticleBlueprint& blueprint, std::uint64_t maxParticleCount);

        std::shared_ptr<ParticleEmitter> createEmitter();

        /// Gets a free particle inside the particle pool. Do not keep this pointer. Returns nullptr if none is available
        /// Not thread-safe
        Particle* getFreeParticle();

        Pipeline& getRenderingPipeline();
        ComputePipeline& getComputePipeline();

        void tick(double deltaTime);

        void onFrame(const Carrot::Render::Context& renderContext) override;

        void renderOpaqueGBuffer(vk::RenderPass pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands) const override;
        void renderTransparentGBuffer(vk::RenderPass pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands) const override;

        void onSwapchainImageCountChange(std::size_t newCount) override;

        void onSwapchainSizeChange(Window& window, int newWidth, int newHeight) override;

        bool isOpaque() const;

    private:
        /// Data on the GPU for a given emitter. Not pointer-stable, do not store
        Carrot::EmitterData& getEmitterData(u32 emitterID);
        friend class ParticleEmitter;

        void render(vk::RenderPass pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands) const;
        void pullDataFromGPU();
        void pushDataToGPU();

    private:
        RenderableParticleBlueprint& blueprint;
        Carrot::Engine& engine;

        Carrot::TrackingAllocator allocator { Carrot::Allocator::getDefault() };

        /// All particles will be kept inside this buffer, sorted by lifetime each frame
        ///  oldParticleCount will keep the number of particles active last frame, while
        ///  usedParticleCount will keep the number of all active particles.
        ///  Before a call to initNewParticles, usedParticleCount-oldParticleCount represent the newly created particle during the frame
        Carrot::BufferView particleBuffer;
        Carrot::BufferAllocation emitterData;
        Carrot::Vector<Carrot::BufferAllocation> emitterDataGraveyard { allocator }; // vector because multiple reallocations could happen on the same frame
        Vector<Particle> particlePool { allocator };
        std::size_t oldParticleCount = 0;
        std::size_t usedParticleCount = 0;

        Vector<std::shared_ptr<ParticleEmitter>> emitters { allocator };
        std::uint64_t maxParticleCount;
        u64 nextEmitterID = 0;

        std::shared_ptr<Pipeline> renderingPipeline = nullptr;
        std::shared_ptr<ComputePipeline> updateParticlesCompute = nullptr;

        Carrot::BufferView statisticsBuffer;
        ParticleStatistics* statistics = nullptr;
        std::atomic<bool> gotUpdated = false;

        void initNewParticles();
        void updateParticles(double deltaTime);
    };
}
