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
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <hb.h>
#include <core/io/IO.h>

#include <hb-blob.hh>
#include <hb-gpu.h>

namespace Carrot::Render {

    Font::Font(VulkanRenderer& renderer,
               const Carrot::IO::Resource& ttfFile
               ):
               renderer(renderer)
    {
        data = ttfFile.readAll();
        hbFontFileBlob = hb_blob_create(reinterpret_cast<const char*>(data.get()), ttfFile.getSize(), hb_memory_mode_t::HB_MEMORY_MODE_READONLY, nullptr, [](void*){});
        hbFace = hb_face_create(hbFontFileBlob, 0);
        hbFont = hb_font_create(hbFace);
        hbBuffer = hb_buffer_create();
        hbGpuDraw = hb_gpu_paint_create_or_fail();
    }

    Font::~Font() {
        hb_gpu_paint_destroy(hbGpuDraw);
        hb_buffer_destroy(hbBuffer);
        hb_font_destroy(hbFont);
        hb_face_destroy(hbFace);
        hb_blob_destroy(hbFontFileBlob);
    }

    RenderableText Font::bake(std::u32string_view text, float pixelSize, TextAlignment horizontalAlignment, TextAlignment verticalAlignment) {
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
        const float emPerPos = 1.0f / scale;

        Carrot::Vector<TextVertex> vertices;
        vertices.resize(glyphCount*4);

        Carrot::Vector<u32> indices;
        indices.resize(glyphCount*6);

        std::unordered_map<hb_codepoint_t, u32> glyphLocations;
        Carrot::Vector<u8> atlasData;

        float width = 0;
        float height = 0;
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

                auto [iter, wasNew] = glyphLocations.try_emplace(glyphID);
                if (wasNew) {
                    hb_gpu_paint_clear(hbGpuDraw);
                    hb_gpu_paint_glyph(hbGpuDraw, hbFont, glyphID);
                    hb_blob_t* encodedText = hb_gpu_paint_encode(hbGpuDraw, nullptr);
                    hb_gpu_paint_recycle_blob(hbGpuDraw, encodedText);

                    u32 glyphLoc = atlasData.size();
                    atlasData.resize(glyphLoc + encodedText->length);
                    memcpy(&atlasData[glyphLoc], encodedText->data, encodedText->length);

                    iter->second = glyphLoc / (sizeof(int16_t)*4);
                }

                TextVertex& a = vertices[glyphIndex * 4 + 0];
                a.position = (glm::vec2{cursorX+xOffset, cursorY+yOffset} + glm::vec2{glyphExtents.x_bearing, glyphExtents.y_bearing}) * scale;
                a.normal = glm::vec2{-1, +1};
                a.glyphLoc = iter->second;
                a.emPerPos = emPerPos;
                a.texcoord = glm::vec2{glyphExtents.x_bearing, glyphExtents.y_bearing};

                TextVertex& b = vertices[glyphIndex * 4 + 1];
                b.position = (glm::vec2{cursorX+xOffset, cursorY+yOffset} + glm::vec2{glyphExtents.x_bearing + glyphExtents.width, glyphExtents.y_bearing}) * scale;
                b.normal = glm::vec2{+1, +1};
                b.glyphLoc = iter->second;
                b.emPerPos = emPerPos;
                b.texcoord = glm::vec2{glyphExtents.x_bearing+ glyphExtents.width, glyphExtents.y_bearing};

                TextVertex& c = vertices[glyphIndex * 4 + 2];
                c.position = (glm::vec2{cursorX+xOffset, cursorY+yOffset} + glm::vec2{glyphExtents.x_bearing + glyphExtents.width, glyphExtents.y_bearing + glyphExtents.height}) * scale;
                c.normal = glm::vec2{+1, -1};
                c.glyphLoc = iter->second;
                c.emPerPos = emPerPos;
                c.texcoord = glm::vec2{glyphExtents.x_bearing + glyphExtents.width, glyphExtents.y_bearing + glyphExtents.height};

                TextVertex& d = vertices[glyphIndex * 4 + 3];
                d.position = (glm::vec2{cursorX+xOffset, cursorY+yOffset} + glm::vec2{glyphExtents.x_bearing, glyphExtents.y_bearing + glyphExtents.height}) * scale;
                d.normal = glm::vec2{-1, -1};
                d.glyphLoc = iter->second;
                d.emPerPos = emPerPos;
                d.texcoord = glm::vec2{glyphExtents.x_bearing, glyphExtents.y_bearing + glyphExtents.height};

                indices[glyphIndex * 6 + 0] = glyphIndex * 4 + 0;
                indices[glyphIndex * 6 + 1] = glyphIndex * 4 + 1;
                indices[glyphIndex * 6 + 2] = glyphIndex * 4 + 2;

                indices[glyphIndex * 6 + 3] = glyphIndex * 4 + 2;
                indices[glyphIndex * 6 + 4] = glyphIndex * 4 + 3;
                indices[glyphIndex * 6 + 5] = glyphIndex * 4 + 0;

                cursorX += xAdvance;
                cursorY += yAdvance;

                width = std::max(width, (float)cursorX*scale);
                height = std::max(height, (float)(glyphExtents.y_bearing+glyphExtents.height)*scale);
            }
        }

        Carrot::BufferAllocation atlasDataGPU = GetResourceAllocator().allocateDeviceBuffer(atlasData.bytes_size(), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);
        atlasDataGPU.view.uploadForFrame(atlasData.data(), atlasData.bytes_size());

        auto mesh = std::make_unique<Carrot::SingleMesh>(
            vertices,
            indices
        );
        Carrot::InstanceData instanceData;
        instanceData.color = glm::vec4{1.0f};
        float xOffset = 0.0f;
        if (horizontalAlignment == TextAlignment::Center) {
            xOffset = -width / 2.0f;
        } else if (horizontalAlignment == TextAlignment::RightOrBottom) {
            xOffset = -width;
        }

        float yOffset = 0.0f;
        if (verticalAlignment == TextAlignment::Center) {
            yOffset = height / 2.0f;
        } else if (verticalAlignment == TextAlignment::RightOrBottom) {
            yOffset = height;
        }

        glm::mat4 localOffset = glm::translate(glm::mat4{1.0f}, glm::vec3 { xOffset, yOffset, 0.0f});
        TextMetrics metrics {
            .width = static_cast<float>(width),
            .height = static_cast<float>(height),
            .basePixelSize = pixelSize,
        };

        return RenderableText(metrics, std::move(instanceData), localOffset, std::move(mesh), std::move(atlasDataGPU));
    }

    void Font::immediateRender(std::u32string_view text, glm::mat4 transform) {
        TODO
    }

    void RenderableText::renderInScene(const Carrot::Render::Context& renderContext) {
        if(!mesh)
            return;

        auto& renderPacket = GetRenderer().makeRenderPacket(Render::PassEnum::OpaqueGBuffer, Render::PacketType::DrawIndexedInstanced, renderContext);
        renderPacket.instanceCount = 1;

        Carrot::InstanceData instanceData = instance;
        instanceData.transform = instance.transform * localOffset;
        instanceData.lastFrameTransform = instance.lastFrameTransform * localOffset;
        renderPacket.useInstance(instanceData);

        renderPacket.pipeline = renderContext.renderer.getOrCreatePipeline("text-rendering", (std::uint64_t)&renderContext.pViewport);
        renderPacket.useMesh(*mesh);

        auto& pushData = renderPacket.addPushConstant("entryPointParams", vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex);
        struct PushData {
            vk::DeviceAddress gpuAtlas;
            u8 is2D;
        } pushDataContents;
        pushDataContents.gpuAtlas = atlas.view.getDeviceAddress();
        pushDataContents.is2D = 0;
        pushData.setData(pushDataContents);


        GetRenderer().render(renderPacket);
    }

    void RenderableText::renderInUI(const Carrot::Render::Context& renderContext, float zOrder) {
        if(!mesh)
            return;

        auto& renderPacket = GetRenderer().makeRenderPacket(Render::PassEnum::UI, Render::PacketType::DrawIndexedInstanced, renderContext);
        renderPacket.instanceCount = 1;

        Carrot::InstanceData instanceData = instance;
        instanceData.transform = instance.transform * localOffset;
        instanceData.lastFrameTransform = instance.lastFrameTransform * localOffset;
        renderPacket.useInstance(instanceData);

        renderPacket.pipeline = renderContext.renderer.getOrCreatePipelineFullPath("resources/pipelines/ui/text.pipeline", (std::uint64_t)&renderContext.pViewport);
        renderPacket.useMesh(*mesh);

        auto& pushData = renderPacket.addPushConstant("entryPointParams", vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex);
        struct PushData {
            vk::DeviceAddress gpuAtlas;
            u8 is2D;
        } pushDataContents;
        pushDataContents.gpuAtlas = atlas.view.getDeviceAddress();
        pushDataContents.is2D = true;
        pushData.setData(pushDataContents);

        renderPacket.transparentGBuffer.zOrder = zOrder;
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
