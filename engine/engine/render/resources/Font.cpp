//
// Created by jglrxavpok on 14/08/2021.
//

#include "Font.h"
#include "engine/render/GBufferDrawData.h"
#include "SingleMesh.h"
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
               const std::vector<std::uint64_t>& renderableCodepoints
               ):
               renderer(renderer)
    {
        data = ttfFile.readAll();
        if(!stbtt_InitFont(&fontInfo, reinterpret_cast<const unsigned char*>(data.get()), 0)) {
            std::string msg = "Failed to initialize font: ";
            msg += ttfFile.getName();
            throw std::runtime_error(msg);
        }
    }

    RenderableText Font::bake(std::u32string_view text, float pixelSize, TextAlignment horizontalAlignment) {
        if(text.empty()) {
            return RenderableText {};
        }
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
        auto bitmap = std::make_shared<Carrot::Render::Texture>(renderer.getVulkanDriver(), vk::Extent3D { w, h, 1 }, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, vk::Format::eR8Unorm);
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

        auto material = GetRenderer().getMaterialSystem().createMaterialHandle();
        auto textureHandle = GetRenderer().getMaterialSystem().createTextureHandle(bitmap);
        material->albedo = textureHandle;
        auto mesh = std::make_unique<Carrot::SingleMesh>(
                                                   std::vector<Carrot::SimpleVertexWithInstanceData>{
                                                           {{0, 0, 0}},
                                                           {{0, 1, 0}},
                                                           {{1, 1, 0}},
                                                           {{1, 0, 0}},
                                                   },
                                                   std::vector<std::uint32_t>{ 2,1,0, 3,2,0 }
        );
        Carrot::InstanceData instanceData;
        instanceData.color = glm::vec4{1.0f};
        float xOffset = 0.0f;
        if (horizontalAlignment == TextAlignment::Center) {
            xOffset = -static_cast<float>(w) / 2.0f;
        }
        glm::mat4 localOffset = glm::translate(glm::mat4{1.0f}, glm::vec3 { xOffset, static_cast<float>(h) / 2.0f, 0.0f})
                * glm::scale(glm::mat4{1.0f}, glm::vec3{static_cast<float>(w), -static_cast<float>(h), 1.0f});
        TextMetrics metrics {
            .width = static_cast<float>(w),
            .height = static_cast<float>(h),
            .baseline = static_cast<float>(baseline),
            .basePixelSize = pixelSize,
        };
        return std::move(RenderableText(metrics, std::move(instanceData), localOffset, std::move(mesh), material));
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

    void RenderableText::render(Carrot::Render::Context renderContext) {
        if(!mesh)
            return;
        auto& renderPacket = GetRenderer().makeRenderPacket(Render::PassEnum::OpaqueGBuffer, Render::PacketType::DrawIndexedInstanced, renderContext);

        renderPacket.pipeline = renderContext.renderer.getOrCreatePipeline("text-rendering", (std::uint64_t)&renderContext.pViewport);
        renderPacket.useMesh(*mesh);
        renderPacket.instanceCount = 1;

        Carrot::InstanceData instanceData = instance;
        instanceData.transform = instance.transform * localOffset;
        instanceData.lastFrameTransform = instance.lastFrameTransform * localOffset;
        renderPacket.useInstance(instanceData);

        Carrot::GBufferDrawData data;
        data.materialIndex = material->getSlot();

        renderPacket.addPerDrawData({&data, 1});

        GetRenderer().render(renderPacket);
    }

    glm::mat4& RenderableText::getTransform() {
        return instance.transform;
    }

    glm::vec4& RenderableText::getColor() {
        return instance.color;
    }

    const glm::mat4& RenderableText::getTransform() const {
        return instance.transform;
    }

    const glm::vec4& RenderableText::getColor() const {
        return instance.color;
    }
}