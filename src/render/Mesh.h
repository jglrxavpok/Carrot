//
// Created by jglrxavpok on 29/11/2020.
//

#pragma once
#include "vulkan/includes.h"
#include "render/Vertex.h"
#include "render/Buffer.h"
#include "Engine.h"

namespace Carrot {
    class Mesh {
    private:
        Carrot::Engine& engine;
        /// <vertices>+<indices> at the end of the vertex list, to better use cache
        unique_ptr<Carrot::Buffer> vertexAndIndexBuffer = nullptr;

        /// How many indices are inside this mesh? Used for the actual drawcall
        uint64_t indexCount = 0;

        /// Index at which indices start inside the buffer
        uint64_t indexStartOffset = 0;

    public:
        explicit Mesh(Carrot::Engine& engine, const std::vector<Carrot::Vertex>& vertices, const std::vector<uint32_t>& indices);

        void bind(const vk::CommandBuffer& buffer) const;
        void draw(const vk::CommandBuffer& buffer, uint32_t instanceCount = 1) const;
    };
}


