//
// Created by jglrxavpok on 14/08/2021.
//

#include "Font.h"
#include <engine/utils/Macros.h>
#include <engine/Engine.h>
#include <engine/render/InstanceData.h>
#include <engine/render/resources/ResourceAllocator.h>
#include <stb_image_write.h>
#include <engine/render/resources/Mesh.h>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Carrot::Render {

    Font::Font(VulkanRenderer& renderer,
               const Carrot::IO::Resource& ttfFile,
               const vector<std::uint64_t>& renderableCodepoints
               ):
               renderer(renderer),
               instanceBuffer(renderer.getEngine().getResourceAllocator().allocateBuffer(Font::MaxInstances * sizeof(Carrot::InstanceData), vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent))
    {
        data = ttfFile.readAll();
        if(!stbtt_InitFont(&fontInfo, reinterpret_cast<const unsigned char*>(data.get()), 0)) {
            std::string msg = "Failed to initialize font: ";
            msg += ttfFile.getName();
            throw std::runtime_error(msg);
        }

        instances = instanceBuffer.map<Carrot::InstanceData>();
    }

    RenderableText Font::bake(std::u32string_view text, float pixelSize) {
        if(instanceCount >= MaxInstances)
            throw std::runtime_error("Hit limit of instances available through this Font object");
        // TODO: this does not take newlines into account, nor any other control character
        std::uint32_t w = 0;
        std::uint32_t h = 0;
        float scale = stbtt_ScaleForPixelHeight(&fontInfo, pixelSize);
        int ascent;
        stbtt_GetFontVMetrics(&fontInfo, &ascent,0,0);
        int baseline = (int) (ascent*scale);

        float xpos = 0.0f;
        for(const auto& c : text) {
            int x0, x1, y0, y1;
            int glyph = stbtt_FindGlyphIndex(&fontInfo, c);
            stbtt_GetGlyphBitmapBox(&fontInfo, glyph, scale, scale, &x0, &y0, &x1, &y1);

            h = std::max(h, static_cast<std::uint32_t>(y1-y0+baseline));

            int advance, lsb;
            stbtt_GetGlyphHMetrics(&fontInfo, glyph, &advance, &lsb);

            xpos += advance * scale;
            // todo: kerning
        }
        w = static_cast<std::uint32_t>(xpos);
        auto bitmap = std::make_unique<Carrot::Render::Texture>(renderer.getVulkanDriver(), vk::Extent3D { w, h, 1 }, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, vk::Format::eR8Unorm);
        std::vector<unsigned char> pixels;
        pixels.resize(w*h);

        xpos = 0.0f;
        for(const auto& c : text) {
            int x0, x1, y0, y1;
            int xCursor = static_cast<std::uint32_t>(xpos);
            int glyph = stbtt_FindGlyphIndex(&fontInfo, c);
            stbtt_GetGlyphBitmapBox(&fontInfo, glyph, scale, scale, &x0, &y0, &x1, &y1);
            if(y0+baseline < 0) { // FIXME: this hack should probably be looked into. For the moment, avoid out-of-bounds accesses
                int charHeight = y1-y0;
                y0 = -baseline;
                y1 = charHeight+y0;
            }
            stbtt_MakeGlyphBitmap(&fontInfo, &pixels[(y0+baseline)*w+x0+xCursor], x1-x0, y1-y0, w, scale, scale, glyph);

            int advance, lsb;
            stbtt_GetGlyphHMetrics(&fontInfo, glyph, &advance, &lsb);

            xpos += advance * scale;
        }
        bitmap->getImage().stageUpload({pixels.data(), pixels.size()});
        stbi_write_png("test-font.png", w, h, 1, pixels.data(), w);

        auto material = renderer.getOrCreatePipeline("text-rendering", static_cast<std::uint64_t>(instanceCount));
        auto mesh = std::make_unique<Carrot::Mesh>(renderer.getVulkanDriver(),
                                                   std::vector<Carrot::SimpleVertexWithInstanceData>{
                                                           {{0, 0, 0}},
                                                           {{0, 1, 0}},
                                                           {{1, 1, 0}},
                                                           {{1, 0, 0}},
                                                   },
                                                   std::vector<std::uint32_t>{ 0,1,2, 2,3,0 }
        );
        std::uint32_t instanceIndex = instanceCount++;
        instances[instanceIndex].color = glm::vec4{1.0f};
        instances[instanceIndex].transform = glm::scale(glm::mat4{1.0f}, glm::vec3{w, h, 0.0f});
        TextMetrics metrics {
            .width = static_cast<float>(w),
            .height = static_cast<float>(h),
            .baseline = static_cast<float>(baseline),
            .basePixelSize = pixelSize,
        };
        return std::move(RenderableText(metrics, instanceBuffer, instanceIndex, std::move(bitmap), std::move(mesh), material, &instances[instanceIndex]));
    }

    std::vector<std::uint64_t>& Font::getAsciiCodepoints() {
        static std::vector<std::uint64_t> codepoints{126-32};
        for (std::size_t i = 0; i < codepoints.size(); ++i) {
            codepoints[i] = i+32;
        }
        return codepoints;
    }

    void Font::immediateRender(std::u32string_view text, glm::mat4 transform) {
        TODO
    }

    void RenderableText::onFrame(Carrot::Render::Context renderContext) {
        auto& renderer = renderContext.renderer;
        renderer.bindTexture(*pipeline, renderContext, *bitmap, 0, 0, nullptr);

    }

    void RenderableText::render(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer cmds) {
        pipeline->bind(pass, renderContext, cmds);
        mesh->bind(cmds);
        cmds.bindVertexBuffers(1, sourceInstanceBuffer->getVulkanBuffer(), sourceInstanceBuffer->getStart() + sizeof(Carrot::InstanceData) * instanceIndex);
        mesh->draw(cmds);
    }

    glm::mat4& RenderableText::getTransform() {
        return instance->transform;
    }
}