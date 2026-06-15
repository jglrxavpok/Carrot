//
// Created by jglrxavpok on 13/06/2026.
//

#pragma once
#include <engine/ecs/components/TransformComponent.h>
#include <engine/render/AsyncResource.hpp>

#include "System.h"

namespace Carrot::ECS {
    class UIRenderSystem final : public RenderSystem<TransformComponent/*TODO*/>, public Identifiable<UIRenderSystem> {
    public:
        explicit UIRenderSystem(World& world);

        UIRenderSystem(const Carrot::DocumentElement& doc, World& world)
            : UIRenderSystem(world) {}

        void onFrame(const Carrot::Render::Context& renderContext) override;
        std::unique_ptr<System> duplicate(World& newOwner) const override;
    public:
        inline static const char* getStringRepresentation() {
            return "UIRenderSystem";
        }

        virtual const char* getName() const override {
            return getStringRepresentation();
        }

    private:
        AsyncResource<Carrot::Pipeline, false> rectanglePipelineResource;
        AsyncResource<Carrot::Pipeline, false> imagePipelineResource;
        AsyncResource<Carrot::Render::Texture, false> testImage;
        std::shared_ptr<Render::TextureHandle> testImageHandle;
        double lastTime = 0.0;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::UIRenderSystem>::getStringRepresentation() {
    return Carrot::ECS::UIRenderSystem::getStringRepresentation();
}
