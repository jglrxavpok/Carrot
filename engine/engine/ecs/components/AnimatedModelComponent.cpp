//
// Created by jglrxavpok on 08/10/2023.
//

#include "AnimatedModelComponent.h"
#include <engine/assets/AssetServer.h>

namespace Carrot::ECS {
    AnimatedModelComponent::AnimatedModelComponent(Entity entity): IdentifiableComponent<AnimatedModelComponent>(std::move(entity)) {}

    AnimatedModelComponent::AnimatedModelComponent(const Carrot::DocumentElement& doc, Entity entity): AnimatedModelComponent(std::move(entity)) {
        auto modelJSON = doc["model"].getAsObject();
        auto& modelPathJSON = doc["model"]["model_path"];
        const std::string modelPath { modelPathJSON.getAsString() };
        queueLoad(Carrot::IO::VFS::Path { modelPath });
        if(auto pRaytracedIter = modelJSON.find("raytraced"); pRaytracedIter != modelJSON.end()) {
            raytraced = pRaytracedIter->second.getAsBool();
        }
    }

    const char *const AnimatedModelComponent::getName() const {
        return Carrot::Identifiable<Carrot::ECS::AnimatedModelComponent>::getStringRepresentation();
    }

    void AnimatedModelComponent::queueLoad(const Carrot::IO::VFS::Path& animatedModelPath) {
        if(!asyncAnimatedModelHandle.isEmpty()) {
            asyncAnimatedModelHandle.forceWait(); // TODO: cancel instead
        }
        asyncAnimatedModelHandle = AsyncHandle(GetAssetServer().loadAnimatedModelInstanceTask(animatedModelPath));
    }

    Carrot::DocumentElement AnimatedModelComponent::serialise() const {
        asyncAnimatedModelHandle.forceWait();
        Carrot::DocumentElement obj;

        Carrot::DocumentElement modelData;
        const auto& resource = waitLoadAndGetOriginatingResource();

        if(resource.isFile()) {
            modelData["model_path"] = resource.getName();
        }

        if(!asyncAnimatedModelHandle.isEmpty()) {
            modelData["raytraced"] = raytraced;
        }

        obj["model"] = modelData;
        return obj;
    }

    std::unique_ptr<Component> AnimatedModelComponent::duplicate(const Entity& newOwner) const {
        auto pClone = std::make_unique<AnimatedModelComponent>(newOwner);
        pClone->asyncAnimatedModelHandle = AsyncHandle(GetAssetServer().loadAnimatedModelInstanceTask(Carrot::IO::VFS::Path{ waitLoadAndGetOriginatingResource().getName() }));
        return pClone;
    }

    const Carrot::IO::Resource& AnimatedModelComponent::waitLoadAndGetOriginatingResource() const {
        asyncAnimatedModelHandle.forceWait();
        return asyncAnimatedModelHandle->getParent().getModel().getOriginatingResource();
    }
}