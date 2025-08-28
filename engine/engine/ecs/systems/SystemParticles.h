//
// Created by jglrxavpok on 20/06/25.
//

#pragma once
#include <core/allocators/TrackingAllocator.h>
#include <engine/ecs/components/ParticleEmitterComponent.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/render/AsyncResource.hpp>
#include <engine/render/particles/Particles.h>

#include "System.h"

namespace Carrot::ECS {
    /**
     * Not named ParticleSystem to avoid confusion with renderer notion of a ParticleSystem
     */
    class SystemParticles final : public Carrot::ECS::RenderSystem<Carrot::ECS::TransformComponent, Carrot::ECS::ParticleEmitterComponent>, public Identifiable<SystemParticles> {
    public:
        explicit SystemParticles(World& world);
        SystemParticles(const Carrot::DocumentElement& doc, World& world);

        void onFrame(const Carrot::Render::Context& renderContext) override;
        std::unique_ptr<System> duplicate(World& newOwner) const override;
        const char* getName() const override;
        void tick(double dt) override;

    public:
        void onEntitiesAdded(const std::vector<EntityID>& entities) override;
        void onEntitiesRemoved(const std::vector<EntityID>& entities) override;
        void onEntitiesUpdated(const std::vector<EntityID>& entities) override;

        void reload() override;
        void unload() override;

    public:
        inline static const char* getStringRepresentation() {
            return "Particles";
        }

    private:
        // From the current entities, fill 'particles' with the systems used by these entities
        void reloadParticleSystems();

        // From the current entities, recreate their emitters. Do not attempt to load new particle systems
        void recreateEmitters();

        struct ParticleSystemStorage {
            bool readyForReload = false;
            AsyncParticleBlueprint pBlueprint; // need to be kept alive for 'particleSystem'
            Carrot::UniquePtr<Carrot::ParticleSystem> pParticleSystem = nullptr;
            std::shared_ptr<IO::FileWatcher> pFileWatcher;
            u64 maxParticles = 0;

            explicit ParticleSystemStorage(const Carrot::IO::VFS::Path& particleFile, u64 maxParticles);
        };

        Carrot::TrackingAllocator allocator { Carrot::Allocator::getDefault() }; // used for allocations below
        std::unordered_map<Carrot::IO::VFS::Path /* path of particle files */, Carrot::UniquePtr<ParticleSystemStorage>> particles;
        std::unordered_set<Carrot::IO::VFS::Path> blueprintsToCheckNextFrame; // keys of particle blueprints to check for loading finish on next frame

    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::SystemParticles>::getStringRepresentation() {
    return Carrot::ECS::SystemParticles::getStringRepresentation();
}