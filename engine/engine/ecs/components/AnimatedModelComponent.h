//
// Created by jglrxavpok on 26/02/2021.
//

#pragma once

#include "Component.h"
#include "engine/render/InstanceData.h"
#include <engine/render/animation/AnimatedModel.h>
#include <engine/render/AsyncResource.hpp>

namespace Carrot::ECS {
    struct AnimatedModelComponent: public IdentifiableComponent<AnimatedModelComponent> {
        using AsyncHandle = Carrot::AsyncResource<Render::AnimatedModel::Handle, false>;

        AsyncHandle asyncAnimatedModelHandle;

        /// Whether to consider this instance for raytracing (shadows and reflections)
        bool raytraced: 1 = true;

        /// Saved version of 'raytraced': used when a scene is unloaded, but not deleted, so the handle is still alive.
        bool raytracedSaved: 1 = true;

        explicit AnimatedModelComponent(Entity entity);
        explicit AnimatedModelComponent(const Carrot::DocumentElement& doc, Entity entity);

        void queueLoad(const Carrot::IO::VFS::Path& animatedModelPath);
        Carrot::DocumentElement serialise() const override;

        const char *const getName() const override;
        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override;

        const Carrot::IO::Resource& waitLoadAndGetOriginatingResource() const;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::AnimatedModelComponent>::getStringRepresentation() {
    return "AnimatedModel";
}