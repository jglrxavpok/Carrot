//
// Created by jglrxavpok on 08/10/2023.
//

#include "AnimatedModelRenderSystem.h"
#include <engine/vulkan/CustomTracyVulkan.h>
#include <engine/render/GBufferDrawData.h>
#include <engine/render/resources/ResourceAllocator.h>
#include <engine/render/InstanceData.h>
#include <engine/render/RenderPacket.h>

namespace Carrot::ECS {
    AnimatedModelRenderSystem::AnimatedModelRenderSystem(const rapidjson::Value& json, World& world): RenderSystem<TransformComponent, AnimatedModelComponent>(world) {

    }

    void AnimatedModelRenderSystem::onFrame(Carrot::Render::Context renderContext) {
        renderedModels.clear();
        parallelForEachEntity([&](Entity& entity, TransformComponent& transform, AnimatedModelComponent& modelComp) {
            ZoneScopedN("Per entity");
            if(!entity.isVisible()) {
                return;
            }

            if (modelComp.asyncAnimatedModelHandle.isReady()) {
                Carrot::AnimatedInstanceData& instanceData = modelComp.asyncAnimatedModelHandle->getData();
                instanceData.lastFrameTransform = transform.lastFrameGlobalTransform;
                instanceData.transform = transform.toTransformMatrix();
                instanceData.uuid = entity.getID();

                // TODO: instanceData.color = modelComp.color;

                // TODO: update TLAS properties

                renderedModels.insert(&modelComp.asyncAnimatedModelHandle->getParent());
            }
        });

        for(Render::AnimatedModel* pModel : renderedModels) {
            pModel->onFrame(renderContext);
        }
    }

    void AnimatedModelRenderSystem::tick(double deltaTime) {
        time += deltaTime;
    }

    std::unique_ptr<System> AnimatedModelRenderSystem::duplicate(World& newOwner) const {
        return std::make_unique<AnimatedModelRenderSystem>(newOwner);
    }

    void AnimatedModelRenderSystem::reload() {
        time = 0.0;
        forEachEntity([&](Entity& entity, TransformComponent& transform, AnimatedModelComponent& modelComp) {
            // TODO: enable TLAS
        });
    }

    void AnimatedModelRenderSystem::unload() {
        forEachEntity([&](Entity& entity, TransformComponent& transform, AnimatedModelComponent& modelComp) {
            // TODO: disable TLAS
        });
        time = 0.0;
    }
}
