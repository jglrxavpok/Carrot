//
// Created by jglrxavpok on 14/08/2021.
//

#pragma once

#include <engine/vulkan/VulkanDriver.h>
#include <engine/io/Resource.h>
#include <engine/render/resources/Mesh.h>
#include <glm/glm.hpp>
#include <stb_truetype.h>
#include <engine/render/resources/BufferView.h>

namespace Carrot::Render {
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

        void onFrame(Carrot::Render::Context renderContext);
        void render(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer cmds);

        glm::mat4& getTransform();
        const glm::mat4& getTransform() const;

        glm::vec4& getColor();
        const glm::vec4& getColor() const;
        const TextMetrics& getMetrics() const { return metrics; }

        RenderableText& operator=(RenderableText&& other) = default;

    private:
        explicit RenderableText(
                TextMetrics metrics,
                Carrot::BufferView& instancesBuffer,
                std::uint32_t instanceIndex,
                std::unique_ptr<Carrot::Render::Texture>&& bitmap,
                std::unique_ptr<Carrot::Mesh>&& mesh,
                std::shared_ptr<Carrot::Pipeline> pipeline,
                Carrot::InstanceData* instance
                ):
            metrics(metrics), instanceIndex(instanceIndex), sourceInstanceBuffer(&instancesBuffer), bitmap(std::move(bitmap)), mesh(std::move(mesh)), pipeline(std::move(pipeline)), instance(instance) {};
        std::unique_ptr<Carrot::Render::Texture> bitmap = nullptr;
        std::unique_ptr<Carrot::Mesh> mesh = nullptr;
        Carrot::InstanceData* instance = nullptr;
        Carrot::BufferView* sourceInstanceBuffer = nullptr;
        std::uint32_t instanceIndex = 0;

        std::shared_ptr<Carrot::Pipeline> pipeline = nullptr;
        TextMetrics metrics;

        friend class Font;
    };

    class Font {
    public:
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
        Carrot::BufferView instanceBuffer;
        std::uint32_t instanceCount = 0;
        stbtt_fontinfo fontInfo;
        Carrot::InstanceData* instances = nullptr;
    };

}
