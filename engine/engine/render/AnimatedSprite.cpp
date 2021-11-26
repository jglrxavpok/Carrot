//
// Created by jglrxavpok on 05/08/2021.
//

#include "AnimatedSprite.h"
#include <engine/utils/Macros.h>
#include <cmath>

namespace Carrot::Render {
    AnimatedSprite::AnimatedSprite(Carrot::VulkanRenderer& renderer,
                                   Carrot::Render::Texture::Ref texture,
                                   TextureAtlas atlas,
                                   std::vector<glm::ivec2> indices,
                                   float animationDuration): Sprite(renderer, texture), atlas(std::move(atlas)), indices(std::move(indices)), playbackSpeed(animationDuration) {

    }

    void AnimatedSprite::tick(double deltaTime) {
        if(playing) {
            timer += static_cast<float>(deltaTime);
            timer = std::fmod(timer, playbackSpeed);
            float currentImage = timer / playbackSpeed;
            playbackIndex = static_cast<std::uint32_t>(currentImage * indices.size());
        }
        updateTextureRegion(atlas[indices[playbackIndex]]);
    }

    void AnimatedSprite::play() {
        playing = true;
    }

    void AnimatedSprite::pause() {
        playing = false;
    }

    void AnimatedSprite::restart() {
        playing = true;
        timer = 0.0f;
    }

    void AnimatedSprite::setIndices(const std::vector<glm::ivec2>& newIndices) {
        indices = newIndices;
    }
}
