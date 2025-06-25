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
            if (pStorage->pParticleSystem)
                pStorage->pParticleSystem->onFrame(renderContext);
        }
    }

    void SystemParticles::tick(double dt) {
        // check the loading of pending blueprints
        bool hasLoadedBlueprint = false;
        for (auto keyIter = blueprintsToCheckNextFrame.begin(); keyIter != blueprintsToCheckNextFrame.end();) {
            auto iter = particles.find(*keyIter);
            if (iter == particles.end()) {
                keyIter = blueprintsToCheckNextFrame.erase(keyIter);
                continue;
            }

            ParticleSystemStorage& storage = *iter->second;
            if (storage.pBlueprint.isReady()) {
                if (auto pLoadedBlueprint = storage.pBlueprint.get()) { // can be null if loading failed
                    storage.pParticleSystem = Carrot::makeUnique<Carrot::ParticleSystem>(allocator, GetEngine(), *pLoadedBlueprint, storage.maxParticles);
                    hasLoadedBlueprint = true;
                }

                keyIter = blueprintsToCheckNextFrame.erase(keyIter);
                continue;
            }

            ++keyIter;
        }
        if (hasLoadedBlueprint) {
            recreateEmitters();
        }

        std::atomic_bool needReload = false;
        parallelForEachEntity([&](Entity& entity, TransformComponent& transform, ParticleEmitterComponent& emitter) {
            if (emitter.fileWasModified) {
                needReload = true;
            }
            if (!emitter.emitter) {
                return;
            }

            emitter.emitter->setPosition(transform.computeFinalPosition());
            emitter.emitter->setRotation(transform.computeFinalOrientation());
        });

        if (needReload) { // a particle component was modified and the filepath has changed -> need to reload its particle system
            reloadParticleSystems();
        }
        for (auto& [_, pStorage] : particles) {
            if (pStorage->pParticleSystem) {
                pStorage->pParticleSystem->tick(dt);
            }
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
            emitter.emitter = nullptr; // reset in case of error
            emitter.fileWasModified = false;

            // load each particle system
            const IO::VFS::Path& particleFile = emitter.particleFile;
            if (particleFile.isEmpty()) {
                return;
            }
            if (!GetVFS().exists(particleFile)) {
                return;
            }
            auto [iter, wasNew] = particles.try_emplace(particleFile);
            if (wasNew) {
                constexpr u64 MaxParticles = 1'000'000; // TODO: customizable?
                iter->second = Carrot::makeUnique<ParticleSystemStorage>(allocator, particleFile, MaxParticles);
            }

            ParticleSystemStorage& storage = *iter->second;
            if (!storage.pBlueprint.isReady()) {
                blueprintsToCheckNextFrame.insert(particleFile);
            }
        });

        recreateEmitters();
    }

    void SystemParticles::recreateEmitters() {
        forEachEntity([&](Entity& entity, TransformComponent& transform, ParticleEmitterComponent& emitter) {
            emitter.emitter = nullptr; // reset in case of error

            // load each particle system
            const IO::VFS::Path& particleFile = emitter.particleFile;
            if (particleFile.isEmpty()) {
                return;
            }
            if (!GetVFS().exists(particleFile)) {
                return;
            }
            auto iter = particles.find(particleFile);
            if (iter == particles.end()) {
                return;
            }

            if (ParticleSystem* pParticleSystem = iter->second->pParticleSystem.get()) {
                // make the component reference an emitter inside the system
                emitter.emitter = pParticleSystem->createEmitter();
                emitter.emitter->setRate(emitter.getSpawnRatePerSecond());
                emitter.emitter->setEmittingInWorldSpace(emitter.isInWorldSpace());
            }
        });
    }

    std::unique_ptr<System> SystemParticles::duplicate(World& newOwner) const {
        return std::make_unique<SystemParticles>(newOwner);
    }

    const char* SystemParticles::getName() const {
        return "Particles";
    }

    SystemParticles::ParticleSystemStorage::ParticleSystemStorage(const Carrot::IO::VFS::Path& particleFile, u64 maxParticles)
        : pBlueprint(GetAssetServer().loadParticleBlueprintTask(particleFile)), maxParticles(maxParticles) {
    }

}
