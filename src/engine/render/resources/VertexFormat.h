//
// Created by jglrxavpok on 16/12/2020.
//

#pragma once
#include <vector>
#include "engine/vulkan/includes.h"
#include <string>

namespace Carrot {
    enum class VertexFormat {
        Vertex,
        SkinnedVertex,
        ScreenSpace,
        ComputeSkinnedVertex,
        SimpleVertex,
        Particle,
        Invalid
    };

    VertexFormat getVertexFormat(const std::string& name);
    std::vector<vk::VertexInputBindingDescription> getBindingDescriptions(VertexFormat vertexFormat);
    std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions(VertexFormat vertexFormat);


}
