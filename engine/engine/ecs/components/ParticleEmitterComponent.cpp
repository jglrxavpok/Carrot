//
// Created by jglrxavpok on 20/06/25.
//

#include "ParticleEmitterComponent.h"

#include <engine/render/particles/Particles.h>

namespace Carrot::ECS {
    ParticleEmitterComponent::ParticleEmitterComponent(const Entity& entity): IdentifiableComponent<Carrot::ECS::ParticleEmitterComponent>(entity) {}

    ParticleEmitterComponent::ParticleEmitterComponent(const DocumentElement& doc, const Entity& entity): IdentifiableComponent<Carrot::ECS::ParticleEmitterComponent>(entity) {
        particleFile = doc["file"].getAsString();

        Carrot::DocumentElement::ObjectView objectView = doc.getAsObject();
        if (auto settingsIter = objectView.find("settings"); settingsIter.isValid()) {
            Carrot::DocumentElement::ObjectView settingsView = settingsIter->second.getAsObject();
            if (auto spawnRateIter = settingsView.find("spawn_rate"); spawnRateIter.isValid()) {
                settings.spawnRatePerSecond = spawnRateIter->second.getAsDouble();
            }

            if (auto worldSpaceIter = settingsView.find("emit_in_world_space"); worldSpaceIter.isValid()) {
                settings.worldSpace = worldSpaceIter->second.getAsBool();
            }
        }
    }

    std::unique_ptr<Component> ParticleEmitterComponent::duplicate(const Entity& newOwner) const {
        std::unique_ptr<ParticleEmitterComponent> pCopy = std::make_unique<ParticleEmitterComponent>(newOwner);
        pCopy->particleFile = particleFile;
        pCopy->settings = settings;
        return pCopy;
    }

    Carrot::DocumentElement ParticleEmitterComponent::serialise() const {
        Carrot::DocumentElement doc;
        doc["file"] = particleFile.toString();
        doc["settings"]["spawn_rate"] = settings.spawnRatePerSecond;
        doc["settings"]["emit_in_world_space"] = settings.worldSpace;
        return doc;
    }

    const char* const ParticleEmitterComponent::getName() const {
        return Carrot::Identifiable<ParticleEmitterComponent>::getStringRepresentation();
    }

    float ParticleEmitterComponent::getSpawnRatePerSecond() const {
        return settings.spawnRatePerSecond;
    }

    void ParticleEmitterComponent::setSpawnRatePerSecond(float newRate) {
        if (newRate != settings.spawnRatePerSecond && emitter) {
            emitter->setRate(newRate);
        }
        settings.spawnRatePerSecond = newRate;
    }

    bool ParticleEmitterComponent::isInWorldSpace() const {
        return settings.worldSpace;
    }

    void ParticleEmitterComponent::setInWorldSpace(bool inWorldSpace) {
        if (inWorldSpace != settings.worldSpace && emitter) {
            emitter->setEmittingInWorldSpace(inWorldSpace);
        }
        settings.worldSpace = inWorldSpace;
    }
}
