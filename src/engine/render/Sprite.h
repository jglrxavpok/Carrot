//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once
#include "engine/render/resources/Buffer.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/render/resources/Texture.h"
#include "engine/math/Rect2D.hpp"
#include "engine/render/InstanceData.h"
#include <glm/ext/quaternion_common.hpp>
#include <glm/detail/type_quat.hpp>
#include "engine/render/VulkanRenderer.h"
#include "engine/render/resources/BufferView.h"

namespace Carrot::Render {
    class Sprite {
    public:
        glm::mat4 parentTransform{1.0f};
        glm::vec3 position{0.0};
        glm::vec2 size{1.0f};
        glm::quat rotation = glm::identity<glm::quat>();

        explicit Sprite(Carrot::VulkanRenderer& renderer, Carrot::Render::Texture::Ref texture, Carrot::Math::Rect2Df textureRegion = Carrot::Math::Rect2Df { 0.0f, 0.0f, 1.0f, 1.0f });

        virtual ~Sprite() = default;

    public:
        void updateTextureRegion(const Carrot::Math::Rect2Df& newRegion);
        glm::mat4 computeTransformMatrix() const;

    public:
        virtual void tick(double deltaTime) {}
        void onFrame(Carrot::Render::Context renderContext) const;
        void soloGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) const;

    public:
        Carrot::VulkanRenderer& getRenderer() { return renderer; }

    public:
        static void cleanup();

    private:
        Carrot::VulkanRenderer& renderer;
        Carrot::Render::Texture::Ref texture;
        Carrot::Math::Rect2Df textureRegion;
        Carrot::BufferView instanceBuffer;
        Carrot::InstanceData* instanceData = nullptr;

        std::shared_ptr<Carrot::Pipeline> renderingPipeline;

        static std::unique_ptr<Carrot::Mesh> spriteMesh;
        static Carrot::Mesh& getSpriteMesh(Carrot::Engine& engine);
    };
}
