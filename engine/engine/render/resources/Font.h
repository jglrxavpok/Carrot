//
// Created by jglrxavpok on 14/08/2021.
//

#pragma once

#include <core/io/Resource.h>
#include <engine/render/resources/Mesh.h>
#include <glm/glm.hpp>
#include "engine/render/InstanceData.h"
#include "engine/render/MaterialSystem.h"

// Harbuzz forward declarations
struct hb_blob_t;
struct hb_face_t;
struct hb_font_t;
struct hb_buffer_t;
struct hb_raster_draw_t;
struct hb_gpu_draw_t;
struct hb_gpu_paint_t;

namespace Carrot::Render {
    class MaterialHandle;

    struct TextMetrics {
        float width = 0.0f;
        float height = 0.0f;
        float baseline = 0.0f;
        float basePixelSize = 0.0f;
    };

    enum class TextAlignment {
        LeftOrTop,
        Center,
        RightOrBottom
    };

    class RenderableText {
    public:
        RenderableText(RenderableText&& text) = default;
        explicit RenderableText() = default;

        void renderInScene(const Carrot::Render::Context& renderContext);
        void renderInUI(const Carrot::Render::Context& renderContext, float zOrder);

        glm::mat4& getTransform();
        const glm::mat4& getTransform() const;

        glm::vec4& getColor();
        const glm::vec4& getColor() const;
        const TextMetrics& getMetrics() const { return metrics; }

        RenderableText& operator=(RenderableText&& other) = default;

    private:
        explicit RenderableText(
                TextMetrics metrics,
                Carrot::InstanceData&& instanceData,
                glm::mat4 localOffset,
                std::unique_ptr<Carrot::Mesh>&& mesh,
                Carrot::BufferAllocation atlas
                ):
            metrics(metrics), localOffset(localOffset), mesh(std::move(mesh)), instance(std::move(instanceData)), atlas(std::move(atlas)) {};
        glm::mat4 localOffset {1.0f};
        Carrot::InstanceData instance;
        std::unique_ptr<Carrot::Mesh> mesh = nullptr;

        TextMetrics metrics;
        Carrot::BufferAllocation atlas;

        friend class Font;
    };

    class Font {
    public:
        using Ref = std::shared_ptr<Font>;

        static constexpr std::uint32_t MaxInstances = 256;
        static constexpr float DefaultPixelSize = 64.0f;

        // TODO: Support font fallback
        explicit Font(Carrot::VulkanRenderer& renderer, const Carrot::IO::Resource& ttfFile);
        ~Font();

    public:
        RenderableText bake(std::u32string_view text, float pixelSize = DefaultPixelSize, TextAlignment horizontalAlignment = TextAlignment::Center, TextAlignment verticalAlignment = TextAlignment::Center);
        void immediateRender(std::u32string_view text, glm::mat4 transform);

    private:
        std::unique_ptr<std::uint8_t[]> data = nullptr; // must be kept alive for harfbuzz to work
        hb_blob_t* hbFontFileBlob = nullptr;
        hb_face_t* hbFace = nullptr;
        hb_font_t* hbFont = nullptr;
        hb_buffer_t* hbBuffer = nullptr;
        hb_gpu_paint_t* hbGpuDraw = nullptr;
        Carrot::VulkanRenderer& renderer;
    };

}
