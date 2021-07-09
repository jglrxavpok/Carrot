//
// Created by jglrxavpok on 06/07/2021.
//

#pragma once

#include <memory>
#include "engine/utils/UUID.h"

namespace Carrot::Render {
    struct TextureSize {
        enum class Type {
            SwapchainProportional,
            Fixed,
        } type = Type::SwapchainProportional;

        double width = 1.0;
        double height = 1.0;
        double depth = 1.0;

        TextureSize() = default;
        TextureSize(const TextureSize&) = default;

        TextureSize(const vk::Extent3D& extent) {
            type = Type::Fixed;
            width = extent.width;
            height = extent.height;
            depth = extent.depth;
        }

        TextureSize(const vk::Extent2D& extent) {
            type = Type::Fixed;
            width = extent.width;
            height = extent.height;
            depth = 1.0;
        }

        TextureSize& operator=(const TextureSize& toCopy) {
            type = toCopy.type;
            width = toCopy.width;
            height = toCopy.height;
            depth = toCopy.depth;
            return *this;
        }
    };

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
            size = toCopy.size;
            format = toCopy.format;
            isSwapchain = toCopy.isSwapchain;
        }

        explicit FrameResource(const FrameResource* parent) {
            size = parent->size;
            format = parent->format;
            isSwapchain = parent->isSwapchain;
            rootID = parent->rootID;
            parentID = parent->id;
        };

        TextureSize size;
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