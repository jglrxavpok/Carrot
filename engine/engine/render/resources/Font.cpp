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
#include <engine/render/resources/Mesh.h>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <hb-raster.h>
#include <hb.h>
#include <hb-raster-image.hh>
#include <core/io/IO.h>

#include <hb-blob.hh>

namespace Carrot::Render {

    Font::Font(VulkanRenderer& renderer,
               const Carrot::IO::Resource& ttfFile,
               const std::vector<std::uint64_t>& renderableCodepoints
               ):
               renderer(renderer)
    {
        data = ttfFile.readAll();
        hbFontFileBlob = hb_blob_create(reinterpret_cast<const char*>(data.get()), ttfFile.getSize(), hb_memory_mode_t::HB_MEMORY_MODE_READONLY, nullptr, [](void*){});
        hbFace = hb_face_create(hbFontFileBlob, 0);
        hbFont = hb_font_create(hbFace);
        hbBuffer = hb_buffer_create();
        hbDrawer = hb_raster_draw_create_or_fail();
    }

    Font::~Font() {
        hb_raster_draw_destroy(hbDrawer);
        hb_buffer_destroy(hbBuffer);
        hb_font_destroy(hbFont);
        hb_face_destroy(hbFace);
        hb_blob_destroy(hbFontFileBlob);
    }


    RenderableText Font::bake(std::u32string_view text, float pixelSize, TextAlignment horizontalAlignment) {
        if(text.empty()) {
            return RenderableText {};
        }
        hb_buffer_reset(hbBuffer);
        hb_buffer_add_utf32(hbBuffer, reinterpret_cast<const uint32_t*>(text.data()), text.size(), 0, -1);
        hb_buffer_guess_segment_properties(hbBuffer);

        hb_shape(hbFont, hbBuffer, nullptr, 0);

        // TODO: implement line breaks
        u32 glyphCount;
        const hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(hbBuffer, &glyphCount);
        const hb_glyph_position_t* glyphPositions = hb_buffer_get_glyph_positions(hbBuffer, &glyphCount);


        int fontHeight;
        hb_font_get_scale(hbFont, nullptr, &fontHeight);

        const float scale = pixelSize / fontHeight;

        hb_raster_draw_clear(hbDrawer);
        hb_raster_draw_set_scale_factor(hbDrawer, 1.0f/scale, 1.0f/scale);
        hb_raster_draw_set_transform(hbDrawer, 1, 0, 0, 1, 0, 0);

        u32 width = 0;
        u32 height = 0;
        {
            hb_position_t cursorX = 0;
            hb_position_t cursorY = 0;
            for (u32 glyphIndex = 0; glyphIndex < glyphCount; glyphIndex++) {
                hb_codepoint_t glyphID = glyphInfo[glyphIndex].codepoint;
                hb_position_t xAdvance = glyphPositions[glyphIndex].x_advance;
                hb_position_t yAdvance = glyphPositions[glyphIndex].y_advance;
                hb_position_t xOffset = glyphPositions[glyphIndex].x_offset;
                hb_position_t yOffset = glyphPositions[glyphIndex].y_offset;

                hb_glyph_extents_t glyphExtents;
                hb_font_get_glyph_extents(hbFont, glyphID, &glyphExtents);
                hb_raster_draw_set_transform(hbDrawer, 1, 0, 0, 1, cursorX+xOffset, cursorY+yOffset);
                hb_raster_draw_glyph(hbDrawer, hbFont, glyphID);

                cursorX += xAdvance;
                cursorY += yAdvance;
            }
        }
        // Raster glyphs to image
        // TODO: use Slug algorithm
        hb_raster_image_t* mask = hb_raster_draw_render(hbDrawer);
        hb_raster_draw_recycle_image(hbDrawer, mask);

        width = mask->extents.width;
        height = mask->extents.height-mask->extents.y_origin;

        Carrot::Vector<u8> pixels;
        pixels.resize(width * height);
        pixels.fill(0);

        float baseline = mask->extents.y_origin;
        for (u32 y = 0; y < mask->extents.height; y++) {
            for (u32 x = 0; x < mask->extents.width; x++) {
                // same computation as hb_raster_image_t::serialize_to_png_or_fail
                pixels[x + y * width] = mask->buffer.arrayZ[x + (mask->extents.height - 1 - y) * mask->extents.stride];
            }
        }

        // create texture and pixel storage
        std::shared_ptr<Texture> bitmap = std::make_shared<Carrot::Render::Texture>(renderer.getVulkanDriver(), vk::Extent3D { width, height, 1 }, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, vk::Format::eR8Unorm);
        bitmap->getImage().stageUpload({pixels.data(), static_cast<std::size_t>(pixels.bytes_size())});

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
            xOffset = -static_cast<float>(width) / 2.0f;
        }
        xOffset += mask->extents.x_origin;

        glm::mat4 localOffset = glm::translate(glm::mat4{1.0f}, glm::vec3 { xOffset, static_cast<float>(height) / 2.0f + baseline, 0.0f})
                * glm::scale(glm::mat4{1.0f}, glm::vec3{static_cast<float>(width), -static_cast<float>(height), 1.0f});
        TextMetrics metrics {
            .width = static_cast<float>(width),
            .height = static_cast<float>(height),
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
