//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once
#include "vulkan/includes.h"
#include <glm/common.hpp>

namespace Carrot {
    enum class LightType: std::uint32_t {
        Point,
        Directional,
// TODO:        Spot,
    };

    struct Light {
        alignas(16) glm::vec3 position{0.0f};
        float intensity = 1.0f;

        alignas(16) glm::vec3 direction{1.0f};
        LightType type = LightType::Point;

        alignas(16) glm::vec3 color{1.0f};
        bool enabled = false;
    };

    struct RaycastedShadowingLightBuffer {
        alignas(16) glm::vec3 ambient{0.1f};

    private:
        std::uint32_t lightCount = 0;

    public:
        alignas(16) Light lights[];

        static std::unique_ptr<RaycastedShadowingLightBuffer> create(std::uint32_t lightCount) {
            auto* lightBuffer = (RaycastedShadowingLightBuffer*) malloc(sizeof(RaycastedShadowingLightBuffer) + lightCount * sizeof(Light));

            // default init
            lightBuffer->ambient = glm::vec3(0.1f);
            lightBuffer->lightCount = lightCount;
            for (int i = 0; i < lightCount; ++i) {
                lightBuffer->lights[i] = Light{};
            }

            return std::unique_ptr<RaycastedShadowingLightBuffer>(lightBuffer);
        }

        size_t getStructSize() const {
            return sizeof(RaycastedShadowingLightBuffer) + sizeof(Light) * lightCount;
        }
    };
}