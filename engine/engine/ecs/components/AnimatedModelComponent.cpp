//
// Created by jglrxavpok on 08/10/2023.
//

#include "AnimatedModelComponent.h"
#include <engine/assets/AssetServer.h>

namespace Carrot::ECS {
    AnimatedModelComponent::AnimatedModelComponent(Entity entity): IdentifiableComponent<AnimatedModelComponent>(std::move(entity)) {}

    AnimatedModelComponent::AnimatedModelComponent(const rapidjson::Value& json, Entity entity): AnimatedModelComponent(std::move(entity)) {
        auto& modelPathJSON = json["model"]["model_path"];
        const std::string modelPath = std::string{ modelPathJSON.GetString(), modelPathJSON.GetStringLength() };
        queueLoad(Carrot::IO::VFS::Path { modelPath });
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

    rapidjson::Value AnimatedModelComponent::toJSON(rapidjson::Document& doc) const {
        asyncAnimatedModelHandle.forceWait();
        rapidjson::Value obj{rapidjson::kObjectType};

        rapidjson::Value modelData(rapidjson::kObjectType);
        const auto& resource = waitLoadAndGetOriginatingResource();

        if(resource.isFile()) {
            rapidjson::Value modelPath{resource.getName(), doc.GetAllocator()};
            modelData.AddMember("model_path", modelPath, doc.GetAllocator());
        }

        obj.AddMember("model", modelData, doc.GetAllocator());
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