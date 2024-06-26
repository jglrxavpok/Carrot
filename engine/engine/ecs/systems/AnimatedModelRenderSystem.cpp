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
        Async::SpinLock renderedModelsLock;
        renderedModels.clear();
        parallelForEachEntity([&](Entity& entity, TransformComponent& transform, AnimatedModelComponent& modelComp) {
            ZoneScopedN("Per entity");

            if (modelComp.asyncAnimatedModelHandle.isReady()) {
                if(!entity.isVisible()) {
                    modelComp.asyncAnimatedModelHandle->visible = false;
                    return;
                }
                modelComp.asyncAnimatedModelHandle->visible = true;

                Carrot::AnimatedInstanceData& instanceData = modelComp.asyncAnimatedModelHandle->getData();
                instanceData.lastFrameTransform = transform.lastFrameGlobalTransform;
                instanceData.transform = transform.toTransformMatrix();
                instanceData.uuid = entity.getID();
                instanceData.raytraced = modelComp.raytraced;

                // TODO: instanceData.color = modelComp.color;

                // TODO: update TLAS properties

                Carrot::Render::AnimatedModel* pAnimatedModel = &modelComp.asyncAnimatedModelHandle->getParent();
                {
                    Async::LockGuard g { renderedModelsLock };
                    renderedModels.insert(pAnimatedModel);
                }
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
        // no need to reactivate raytracing nor visibility, "onFrame" will do it for us
        forEachEntity([&](Entity& entity, TransformComponent& transform, AnimatedModelComponent& modelComp) {
            modelComp.raytraced = modelComp.raytracedSaved;
        });
    }

    void AnimatedModelRenderSystem::unload() {
        forEachEntity([&](Entity& entity, TransformComponent& transform, AnimatedModelComponent& modelComp) {
            if (modelComp.asyncAnimatedModelHandle.isReady()) {
                modelComp.asyncAnimatedModelHandle->visible = false;
            }
            modelComp.raytracedSaved = modelComp.raytraced;
            modelComp.raytraced = false; // deactivate instance in raytracing, to avoid shadows and reflections still showing the instance
        });
        time = 0.0;
    }
}
