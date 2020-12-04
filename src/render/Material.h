//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once

#include "Engine.h"
#include "IDTypes.h"

namespace Carrot {
    class Material {
    private:
        MaterialID id = 0;
        Carrot::Engine& engine;
        shared_ptr<Pipeline> pipeline = nullptr;
        TextureID textureID{0};

    public:
        explicit Material(Carrot::Engine& engine, const string& materialName);

        MaterialID getMaterialID() const;
        TextureID getTextureID() const;
        void setTextureID(TextureID texID);

        void bindForRender(const uint32_t imageIndex, vk::CommandBuffer& commands) const;
    };
}
