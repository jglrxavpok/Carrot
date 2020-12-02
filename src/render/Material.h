//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once

#include "Engine.h"

namespace Carrot {
    class Material {
    private:
        Carrot::Engine& engine;
        shared_ptr<Pipeline> pipeline = nullptr;
        vector<vk::DescriptorSet> descriptorSets{};

    public:
        explicit Material(Carrot::Engine& engine);

        void bind(const string& shaderName, const string& textureName, uint32_t textureIndex, uint32_t bindingIndex);
        void bindDefaultValues();

        void bindForRender(const uint32_t imageIndex, vk::CommandBuffer& commands) const;
    };
}
