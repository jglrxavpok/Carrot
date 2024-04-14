//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once
#include "engine/render/resources/Buffer.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/render/resources/Texture.h"
#include "core/math/Rect2D.hpp"
#include "engine/render/InstanceData.h"
#include <glm/ext/quaternion_common.hpp>
#include <glm/detail/type_quat.hpp>
#include "engine/render/VulkanRenderer.h"
#include "engine/render/resources/BufferView.h"

namespace sol {
    class state;
}

namespace Carrot::Render {
    class Sprite {
    public:
        glm::mat4 parentTransform{1.0f};
        glm::vec3 position{0.0};
        glm::vec2 size{1.0f};
        glm::vec4 color{1.0f};
        glm::quat rotation = glm::identity<glm::quat>();

        Sprite();
        explicit Sprite(Carrot::Render::Texture::Ref texture, Carrot::Math::Rect2Df textureRegion = Carrot::Math::Rect2Df { 0.0f, 0.0f, 1.0f, 1.0f });

        Sprite(const Sprite& toCopy) = default;
        Sprite(Sprite&& toMove) = default;

        Sprite& operator=(const Sprite& toCopy) = default;
        Sprite& operator=(Sprite&& toMove) = default;

        std::shared_ptr<Sprite> duplicate() const;

        virtual ~Sprite() = default;

    public:
        void updateTextureRegion(const Carrot::Math::Rect2Df& newRegion);
        glm::mat4 computeTransformMatrix() const;

    public:
        virtual void tick(double deltaTime) {}
        void onFrame(const Carrot::Render::Context& renderContext) const;
        [[deprecated("Rendering done via onFrame")]] void soloGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) const {}

    public:
        const Texture& getTexture() const { return *texture; }
        Texture::Ref getTextureRef() { return texture; }
        Math::Rect2Df& getTextureRegion() { return textureRegion; }
        const Math::Rect2Df& getTextureRegion() const { return textureRegion; }
        Pipeline& getRenderingPipeline() const { return *renderingPipeline; }

    public:
        void setTexture(Texture::Ref texture);

    public:
        static void cleanup();

        static void registerUsertype(sol::state& destination);

    private:
        Carrot::Render::Texture::Ref texture;
        Carrot::Math::Rect2Df textureRegion;
        std::shared_ptr<Carrot::Render::MaterialHandle> material;

        std::shared_ptr<Carrot::Pipeline> renderingPipeline;

        static std::unique_ptr<Carrot::Mesh> spriteMesh;
        static Carrot::Mesh& getSpriteMesh(Carrot::Engine& engine);
    };
}
