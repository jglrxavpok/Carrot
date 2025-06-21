//
// Created by jglrxavpok on 20/06/25.
//

#pragma once

#include "Component.h"
#include <core/io/vfs/VirtualFileSystem.h>

namespace Carrot {
    class ParticleEmitter;
}

namespace Carrot::ECS {
    class ParticleEmitterComponent: public IdentifiableComponent<ParticleEmitterComponent> {
    public:
        Carrot::IO::VFS::Path particleFile;

        std::shared_ptr<Carrot::ParticleEmitter> emitter;

        explicit ParticleEmitterComponent(const Entity& entity);
        explicit ParticleEmitterComponent(const DocumentElement& doc, const Entity& entity);

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override;
        Carrot::DocumentElement serialise() const override;
        const char* const getName() const override;

    public: // settings
        /// How many particles are spawned per second?
        float getSpawnRatePerSecond() const;

        /// Change how many particles are spawned per second
        void setSpawnRatePerSecond(float newRate);

        /// Should the particles follow the position & rotation of the emitter (false) or be independent (true) ?
        bool isInWorldSpace() const;
        void setInWorldSpace(bool inWorldSpace);

    private:
        // particle settings, private to update emitter only when required
        struct Settings {
            float spawnRatePerSecond = 1.0f;
            bool worldSpace = false; // should the particles follow the position & rotation of the emitter (false) or be independent (true)
        } settings;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::ParticleEmitterComponent>::getStringRepresentation() {
    return "ParticleEmitter";
}
