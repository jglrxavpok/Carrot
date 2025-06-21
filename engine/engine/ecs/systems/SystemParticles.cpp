//
// Created by jglrxavpok on 20/06/25.
//

#include "SystemParticles.h"

#include <engine/Engine.h>
#include <engine/render/particles/Particles.h>

namespace Carrot::ECS {
    SystemParticles::SystemParticles(World& world)
    : RenderSystem<Carrot::ECS::TransformComponent, Carrot::ECS::ParticleEmitterComponent>(world)
    {}

    SystemParticles::SystemParticles(const Carrot::DocumentElement& doc, World& world)
    : RenderSystem<Carrot::ECS::TransformComponent, Carrot::ECS::ParticleEmitterComponent>(doc, world)
    {}

    void SystemParticles::onFrame(Carrot::Render::Context renderContext) {
        for (auto& [_, pStorage] : particles) {
            pStorage->particleSystem.onFrame(renderContext);
        }
    }

    void SystemParticles::tick(double dt) {
        parallelForEachEntity([&](Entity& entity, TransformComponent& transform, ParticleEmitterComponent& emitter) {
            if (!emitter.emitter) {
                return;
            }

            emitter.emitter->setPosition(transform.computeFinalPosition());
            emitter.emitter->setRotation(transform.computeFinalOrientation());
        });
        for (auto& [_, pStorage] : particles) {
            pStorage->particleSystem.tick(dt);
        }
    }

    void SystemParticles::onEntitiesAdded(const std::vector<EntityID>& entities) {
        RenderSystem::onEntitiesAdded(entities);
        reloadParticleSystems();
    }

    void SystemParticles::onEntitiesRemoved(const std::vector<EntityID>& entities) {
        RenderSystem::onEntitiesRemoved(entities);
        reloadParticleSystems();
    }

    void SystemParticles::onEntitiesUpdated(const std::vector<EntityID>& entities) {
        RenderSystem::onEntitiesUpdated(entities);
        reloadParticleSystems();
    }

    void SystemParticles::reload() {
        reloadParticleSystems();
    }
    void SystemParticles::unload() {
        particles.clear();
    }

    void SystemParticles::reloadParticleSystems() {
        particles.clear();
        forEachEntity([&](Entity& entity, TransformComponent& transform, ParticleEmitterComponent& emitter) {
            // load each particle system
            const IO::VFS::Path& particleFile = emitter.particleFile;
            if (particleFile.isEmpty()) {
                emitter.emitter = nullptr;
                return;
            }
            auto [iter, wasNew] = particles.try_emplace(particleFile);
            if (wasNew) {
                constexpr u64 MaxParticles = 1'000'000; // TODO: customizable?
                iter->second = Carrot::makeUnique<ParticleSystemStorage>(allocator, particleFile, MaxParticles);
            }

            // make the component reference an emitter inside the system
            emitter.emitter = iter->second->particleSystem.createEmitter();
            emitter.emitter->setRate(emitter.getSpawnRatePerSecond());
            emitter.emitter->setEmittingInWorldSpace(emitter.isInWorldSpace());
        });
    }

    std::unique_ptr<System> SystemParticles::duplicate(World& newOwner) const {
        return std::make_unique<SystemParticles>(newOwner);
    }

    const char* SystemParticles::getName() const {
        return "Particles";
    }

    SystemParticles::ParticleSystemStorage::ParticleSystemStorage(const Carrot::IO::VFS::Path& particleFile, u64 maxParticles)
    : blueprint(particleFile.toString()), particleSystem(GetEngine(), blueprint, maxParticles)
    {}

}
