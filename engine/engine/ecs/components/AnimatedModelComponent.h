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

        explicit AnimatedModelComponent(Entity entity);
        explicit AnimatedModelComponent(const rapidjson::Value& json, Entity entity);

        void queueLoad(const Carrot::IO::VFS::Path& animatedModelPath);
        rapidjson::Value toJSON(rapidjson::Document& doc) const override;

        const char *const getName() const override;
        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override;

        const Carrot::IO::Resource& waitLoadAndGetOriginatingResource() const;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::AnimatedModelComponent>::getStringRepresentation() {
    return "AnimatedModel";
}