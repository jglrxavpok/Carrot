//
// Created by jglrxavpok on 06/07/2021.
//

#pragma once

#include <memory>
#include "core/utils/UUID.h"

namespace Carrot::Render {
    class GraphBuilder;

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

    enum class ImageOrigin {
        SurfaceSwapchain, // image is part of window swapchain
        Created, // image is created by the engine
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
            imageOrigin = toCopy.imageOrigin;
            owner = toCopy.owner;
            previousLayout = toCopy.previousLayout;
            layout = toCopy.layout;
        }

        explicit FrameResource(const FrameResource* parent) {
            rootID = parent->rootID;
            parentID = parent->id;
            size = parent->size;
            format = parent->format;
            imageOrigin = parent->imageOrigin;
            owner = parent->owner;
            previousLayout = parent->previousLayout;
            layout = parent->layout;
        };

        void updateLayout(vk::ImageLayout newLayout) {
            if(newLayout == layout)
                return;
            previousLayout = layout;
            layout = newLayout;
        }

        TextureSize size;
        vk::Format format = vk::Format::eUndefined;
        ImageOrigin imageOrigin = ImageOrigin::Created;
        Carrot::UUID id;
        Carrot::UUID rootID;
        Carrot::UUID parentID;
        GraphBuilder* owner = nullptr;
        vk::ImageLayout layout = vk::ImageLayout::eUndefined;
        vk::ImageLayout previousLayout = vk::ImageLayout::eUndefined;
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
            FrameResource entityID;
            FrameResource roughnessMetallic;
            FrameResource emissive;
        };

        struct GBufferTransparent {
            FrameResource depthInput;
            FrameResource transparentOutput;
        };

        struct Lighting {
            FrameResource positions;
            FrameResource normals;
            FrameResource albedo;
            FrameResource depthStencil;
            FrameResource flags;
            FrameResource raytracing;
            FrameResource transparent;
            FrameResource roughnessMetallic;
            FrameResource emissive;
            FrameResource skybox;

            FrameResource resolved;
        };

        struct PostProcessing {
            FrameResource postLighting;
            FrameResource postProcessed;
        };

        struct Present {
            Render::FrameResource input;
            Render::FrameResource output;
        };

        struct ComposerRegion {
            bool enabled = true;
            float left = -1.0f;
            float right = +1.0f;
            float bottom = -1.0f;
            float top = +1.0f;
            float depth = 0.0f;
            FrameResource toDraw;
        };

        struct Composer {
            std::vector<ComposerRegion> elements;
            FrameResource depthStencil;
            FrameResource color;
        };
    }
}