//
// Created by jglrxavpok on 06/07/2021.
//

#pragma once

#include <memory>
#include "engine/utils/UUID.h"

namespace Carrot::Render {
    class FrameResource: public std::enable_shared_from_this<FrameResource> {
    public:
        FrameResource() {
            rootID = id;
            parentID = id;
        }

        FrameResource(const FrameResource& toCopy) {
            rootID = toCopy.rootID;
            parentID = toCopy.parentID;
            id = toCopy.id;
            width = toCopy.width;
            height = toCopy.height;
            depth = toCopy.depth;
            format = toCopy.format;
            isSwapchain = toCopy.isSwapchain;
        }

        explicit FrameResource(const FrameResource* parent) {
            width = parent->width;
            height = parent->height;
            depth = parent->depth;
            format = parent->format;
            isSwapchain = parent->isSwapchain;
            rootID = parent->rootID;
            parentID = parent->id;
        };

        std::uint32_t width = -1;
        std::uint32_t height = -1;
        std::uint32_t depth = 1;
        vk::Format format = vk::Format::eUndefined;
        bool isSwapchain = false;
        Carrot::UUID id = Carrot::randomUUID();
        Carrot::UUID rootID = Carrot::randomUUID();
        Carrot::UUID parentID = Carrot::randomUUID();
    };



    // Default pass data
    namespace PassData {
        struct ImGui {
            bool firstFrame = true;
            FrameResource output;
        };

        struct Skybox {
            FrameResource output;
        };

        struct GBuffer {
            FrameResource positions;
            FrameResource normals;
            FrameResource albedo;
            FrameResource depthStencil;
            FrameResource flags;
        };

        struct GResolve {
            FrameResource positions;
            FrameResource normals;
            FrameResource albedo;
            FrameResource depthStencil;
            FrameResource flags;
            FrameResource raytracing;
            FrameResource ui;
            FrameResource skybox;

            FrameResource resolved;
        };

        struct Raytracing {
            FrameResource output;
        };
    }
}