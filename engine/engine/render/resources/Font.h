//
// Created by jglrxavpok on 14/08/2021.
//

#pragma once

#include <core/io/Resource.h>
#include <engine/render/resources/Mesh.h>
#include <glm/glm.hpp>
#include <stb_truetype.h>
#include <engine/render/resources/BufferView.h>
#include "engine/render/InstanceData.h"
#include "engine/render/MaterialSystem.h"

namespace Carrot::Render {
    class MaterialHandle;

    struct TextMetrics {
        float width = 0.0f;
        float height = 0.0f;
        float baseline = 0.0f;
        float basePixelSize = 0.0f;
    };

    class RenderableText {
    public:
        RenderableText(RenderableText&& text) = default;
        explicit RenderableText() = default;

        void render(Carrot::Render::Context renderContext);

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
                std::shared_ptr<Carrot::Render::MaterialHandle> material
                ):
            metrics(metrics), localOffset(localOffset), mesh(std::move(mesh)), material(std::move(material)), instance(std::move(instanceData)) {};
        glm::mat4 localOffset {1.0f};
        Carrot::InstanceData instance;
        std::unique_ptr<Carrot::Mesh> mesh = nullptr; // TODO: use generic square mesh

        std::shared_ptr<Carrot::Render::MaterialHandle> material = nullptr;
        TextMetrics metrics;

        friend class Font;
    };

    class Font {
    public:
        using Ref = std::shared_ptr<Font>;

        static constexpr std::uint32_t MaxInstances = 256;

        // TODO: Support font fallback
        explicit Font(Carrot::VulkanRenderer& renderer, const Carrot::IO::Resource& ttfFile, const std::vector<std::uint64_t>& renderableCodepoints = getAsciiCodepoints());

    public:
        RenderableText bake(std::u32string_view text, float pixelSize = 64.0f);
        void immediateRender(std::u32string_view text, glm::mat4 transform);

    public:
        static std::vector<std::uint64_t>& getAsciiCodepoints();

    private:
        std::unique_ptr<std::uint8_t[]> data = nullptr; // must be kept alive for stb_truetype to work
        Carrot::VulkanRenderer& renderer;
        stbtt_fontinfo fontInfo;
    };

}
