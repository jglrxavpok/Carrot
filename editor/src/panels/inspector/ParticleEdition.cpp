//
// Created by jglrxavpok on 20/06/25.
//

#include <engine/ecs/components/ParticleEmitterComponent.h>

#include "EditorFunctions.h"

namespace Peeler {
    void editParticleEmitterComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::ParticleEmitterComponent*>& components) {
        multiEditField(edition, "Filepath", components,
            +[](Carrot::ECS::ParticleEmitterComponent& c) {
                return c.particleFile;
            },
            +[](Carrot::ECS::ParticleEmitterComponent& c, const Carrot::IO::VFS::Path& path) {
                c.particleFile = path;
            },
            Helpers::Limits<Carrot::IO::VFS::Path> {
                .validityChecker = [](const auto& path) { return Carrot::IO::getFileFormat(path.toString().c_str()) == Carrot::IO::FileFormat::PARTICLE; }
            }
        );

        multiEditField(edition, "Spawn rate (per second)", components,
            +[](Carrot::ECS::ParticleEmitterComponent& c) {
                return c.getSpawnRatePerSecond();
            },
            +[](Carrot::ECS::ParticleEmitterComponent& c, const float& v) {
                c.setSpawnRatePerSecond(v);
            },
            Helpers::Limits<float> {
                .min = 0.0f,
            }
        );

        multiEditField(edition, "Emit in world space", components,
            +[](Carrot::ECS::ParticleEmitterComponent& c) {
                return c.isInWorldSpace();
            },
            +[](Carrot::ECS::ParticleEmitterComponent& c, const bool& v) {
                c.setInWorldSpace(v);
            }
        );
    }
}