//
// Created by jglrxavpok on 16/12/2022.
//

#pragma once

#include <core/render/VertexTypes.h>

namespace Carrot {
    std::vector<vk::VertexInputAttributeDescription> getVertexAttributeDescriptions();
    std::vector<vk::VertexInputBindingDescription> getVertexBindingDescription();

    std::vector<vk::VertexInputAttributeDescription> getComputeSkinnedVertexAttributeDescriptions();
    std::vector<vk::VertexInputBindingDescription> getComputeSkinnedVertexBindingDescription();

    std::vector<vk::VertexInputBindingDescription> getSkinnedVertexBindingDescription();
    std::vector<vk::VertexInputAttributeDescription> getSkinnedVertexAttributeDescriptions();

    std::vector<vk::VertexInputAttributeDescription> getScreenSpaceVertexAttributeDescriptions();
    std::vector<vk::VertexInputBindingDescription> getScreenSpaceVertexBindingDescription();

    std::vector<vk::VertexInputAttributeDescription> getSimpleVertexAttributeDescriptions();
    std::vector<vk::VertexInputBindingDescription> getSimpleVertexBindingDescription();

    std::vector<vk::VertexInputAttributeDescription> getSimpleVertexWithInstanceDataAttributeDescriptions();
    std::vector<vk::VertexInputBindingDescription> getSimpleVertexWithInstanceDataBindingDescription();

    std::vector<vk::VertexInputAttributeDescription> getParticleAttributeDescriptions();
    std::vector<vk::VertexInputBindingDescription> getParticleBindingDescription();
}
