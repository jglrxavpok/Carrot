//
// Created by jglrxavpok on 05/08/2021.
//

#pragma once

#include "TextureAtlas.h"
#include "Sprite.h"

namespace Carrot::Render {
    class AnimatedSprite: public Sprite {
    public:
        explicit AnimatedSprite(Carrot::Render::Texture::Ref texture, TextureAtlas atlas, std::vector<glm::ivec2> indices, float animationDuration);

    public:
        void play();
        void restart();
        void pause();

    public:
        bool isPlaying() const { return playing; }

    public:
        void tick(double deltaTime) override;

        void setIndices(const std::vector<glm::ivec2>& indices);

    private:
        TextureAtlas atlas;
        std::vector<glm::ivec2> indices;
        std::uint32_t playbackIndex = 0;
        float playbackSpeed = 1.0f;
        bool playing = false;
        float timer = 0.0f;
    };
}
