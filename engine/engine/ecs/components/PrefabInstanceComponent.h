#pragma once

#include <core/io/vfs/VirtualFileSystem.h>
#include <engine/ecs/Prefab.h>
#include <engine/ecs/components/Component.h>

#include "ComponentReflection.h"

namespace Carrot::ECS {
    struct PrefabInstanceComponent: public Carrot::ECS::ReflectionComponent<PrefabInstanceComponent> {
        using ReflectionComponent::ReflectionComponent;

        FIELD(AsyncPrefabResource, prefab, "Prefab", {});
        FIELD(Carrot::UUID, childID, "Child ID", Carrot::UUID::null()); //< null UUID if represents root of prefab

        static void applyRewriteRules(Carrot::DocumentElement& doc);
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::PrefabInstanceComponent>::getStringRepresentation() {
    return "PrefabInstanceComponent";
}