//
// Created by jglrxavpok on 06/07/2021.
//

#pragma once

#include <memory>
#include "core/utils/UUID.h"

namespace Carrot {
    class Pipeline;
    struct RenderContext;
}

namespace Carrot::Render {
    class GraphBuilder;
    class Graph;

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

    enum class ResourceType {
        RenderTarget,
        StorageImage,
        StorageBuffer
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
            bufferSize = toCopy.bufferSize;
            format = toCopy.format;
            type = toCopy.type;
            imageOrigin = toCopy.imageOrigin;
            pOriginWindow = toCopy.pOriginWindow;
            owner = toCopy.owner;
            previousLayout = toCopy.previousLayout;
            layout = toCopy.layout;
            name = toCopy.name;
        }

        explicit FrameResource(const FrameResource* parent) {
            rootID = parent->rootID;
            parentID = parent->id;
            size = parent->size;
            bufferSize = parent->bufferSize;
            format = parent->format;
            type = parent->type;
            imageOrigin = parent->imageOrigin;
            pOriginWindow = parent->pOriginWindow;
            owner = parent->owner;
            previousLayout = parent->previousLayout;
            layout = parent->layout;
            name = parent->name;
        };

        void updateLayout(vk::ImageLayout newLayout);

        TextureSize size;
        vk::DeviceSize bufferSize = 0;
        vk::Format format = vk::Format::eUndefined;
        ResourceType type = ResourceType::RenderTarget;
        ImageOrigin imageOrigin = ImageOrigin::Created;
        Window* pOriginWindow = nullptr; //< if image origin is SurfaceSwapchain, points to the window the swapchain is from
        Carrot::UUID id;
        Carrot::UUID rootID;
        Carrot::UUID parentID;
        GraphBuilder* owner = nullptr;
        vk::ImageLayout layout = vk::ImageLayout::eUndefined;
        vk::ImageLayout previousLayout = vk::ImageLayout::eUndefined;
        std::string name;
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
            FrameResource albedo;
            FrameResource positions;
            FrameResource viewSpaceNormalTangents;
            FrameResource flags;
            FrameResource entityID;
            FrameResource metallicRoughnessVelocityXY;
            FrameResource emissiveVelocityZ;
            FrameResource depthStencil;

            void readFrom(Render::GraphBuilder& graph, const GBuffer& other, vk::ImageLayout wantedLayout);
            void writeTo(Render::GraphBuilder& graph, const GBuffer& other);
            void bindInputs(Carrot::Pipeline& pipeline, const Render::Context& context, const Render::Graph& renderGraph, std::uint32_t setID, vk::ImageLayout expectedLayout) const;
        };

        struct HashGridResources {
            FrameResource hashGrid;
            FrameResource constants;
            FrameResource gridPointers;
        };

        struct Lighting {
            // inputs
            GBuffer gBuffer;

            // outputs
            FrameResource directLighting; //< also contains shadows
            std::uint8_t iterationCount = 0;
            FrameResource reflectionsNoisy; // noisy output
            FrameResource ambientOcclusionNoisy; // noisy output
            FrameResource ambientOcclusionSamples; // temporal supersampling
            FrameResource ambientOcclusionHistoryLength; // temporal supersampling
            std::array<FrameResource, 2> ambientOcclusion; // ping-pongs for denoising, use iterationCount % 2 for the index

            HashGridResources hashGrid;
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