//
// Created by jglrxavpok on 17/12/2020.
//

#pragma once

#include <cstdint>
#include <algorithm>
#include <glm/glm.hpp>

namespace Carrot {
    struct Keyframe {
        float timestamp = 0.0f;
        std::vector<glm::mat4> boneTransforms;

        explicit Keyframe(float timestamp = 0.0f): timestamp(timestamp) {}
    };

    struct Animation {
        std::int32_t keyframeCount = 0;
        float duration = 1.0f;
        std::vector<Keyframe> keyframes;
        explicit Animation() = default;
    };

    struct GPUAnimation {
        int keyframeCount;
        float duration;
    };

    struct AnimationMetadata {
        std::size_t index = 0;
        float duration = 1.0f;
    };

}
