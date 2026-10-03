//
// Created by jglrxavpok on 09/07/2021.
//

#include "Composer.h"

#include <engine/Engine.h>

#include <engine/render/RenderPass.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/render/resources/Mesh.h>
#include <engine/vulkan/VulkanDriver.h>
#include <engine/render/TextureRepository.h>

namespace Carrot::Render {
    // TODO: rename
    void ViewportComposition::copyViewportPositions(const ViewportComposition& other) {
        viewports.clear();
        for (const auto& [id, locationToCopy] : other.viewports) {
            ViewportLocation& location = viewports[id];
            location.offset = locationToCopy.offset;
            location.size = locationToCopy.size;
            location.z = locationToCopy.z;

            location.stencil = locationToCopy.stencil;
            location.inheritDepthStencil = locationToCopy.inheritDepthStencil;
            location.renderingOrder = locationToCopy.renderingOrder;
            location.discardIfDepthNotWritten = locationToCopy.discardIfDepthNotWritten;
        }
    }

    void ViewportComposition::serialize(const std::filesystem::path& destination) const {
        Carrot::DocumentElement doc {};

        auto& viewportsObj = doc["viewports"];

        for (const auto& [id, viewport] : viewports) {
            Carrot::DocumentElement& viewportObj = viewportsObj[std::string{id}];
            viewportObj["offset_x"] = viewport.offset.x;
            viewportObj["offset_y"] = viewport.offset.y;
            viewportObj["size_x"] = viewport.size.x;
            viewportObj["size_y"] = viewport.size.y;
            viewportObj["z"] = viewport.z;
            viewportObj["rendering_order"] = viewport.renderingOrder;
            viewportObj["discard_if_depth_not_written"] = viewport.discardIfDepthNotWritten;

            if (viewport.stencil.has_value()) {
                viewportObj["stencil"] = viewport.stencil->serialise();
            }
            if (viewport.inheritDepthStencil.has_value()) {
                viewportObj["inherit_from"] = std::string{viewport.inheritDepthStencil.value()};
            }
        }

        doc.saveToFile(destination, false);
    }

    void ViewportComposition::deserialize(const Carrot::IO::VFS::Path& source) {
        Carrot::DocumentElement doc { Carrot::IO::Resource{source} };
        for (const auto& [name, viewportElement] : doc.at("viewports").getAsObject()) {
            Carrot::Identifier id { name };
            Carrot::Render::ViewportLocation& loc = viewports[id];

            loc.offset.x = static_cast<float>(viewportElement.at("offset_x").getAsDouble());
            loc.offset.y = static_cast<float>(viewportElement.at("offset_y").getAsDouble());
            loc.size.x = static_cast<float>(viewportElement.at("size_x").getAsDouble());
            loc.size.y = static_cast<float>(viewportElement.at("size_y").getAsDouble());
            loc.z = static_cast<float>(viewportElement.at("z").getAsDouble());

            auto objView = viewportElement.getAsObject();
            // older format did not have this value
            if (auto iter = objView.find("rendering_order"); iter.isValid()) {
                loc.renderingOrder = static_cast<i32>(iter->second.getAsInt64());
            }

            if (auto iter = objView.find("inherit_from"); iter.isValid()) {
                loc.inheritDepthStencil = Carrot::Identifier{iter->second.getAsString()};
            }

            if (auto iter = objView.find("discard_if_depth_not_written"); iter.isValid()) {
                loc.discardIfDepthNotWritten = iter->second.getAsBool();
            }

            if (auto stencilIter = objView.find("stencil"); stencilIter.isValid()) {
                loc.stencil.emplace();
                loc.stencil->deserialise(stencilIter->second);
            }
        }
    }

    PassData::ComposerRegion& Composer::add(const ViewportFrameResources& toDraw, float left, float right, float top, float bottom, float z, bool discardIfDepthNotWritten) {
        // graph containing composer pass can be initialised after the 'toDraw' texture has been created
        driver.getEngine().getResourceRepository().getTextureUsages(toDraw.colorOutput.rootID) |= vk::ImageUsageFlagBits::eSampled;
        driver.getEngine().getResourceRepository().getTextureUsages(toDraw.depthStencil.rootID) |= vk::ImageUsageFlagBits::eSampled;

        auto& r = regions.emplace_back();
        r.toDraw = toDraw;
        r.left = left;
        r.right = right;
        r.top = top;
        r.bottom = bottom;
        r.depth = z;
        r.discardWhereDepthIsUnwritten = discardIfDepthNotWritten;
        return r;
    }

    Pass<PassData::Composer>& Composer::appendPass(GraphBuilder& renderGraph) {
        return renderGraph.addPass<PassData::Composer>("composer",
        [this](Render::GraphBuilder& builder, Render::Pass<PassData::Composer>& pass, PassData::Composer& data) {
            vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
            vk::ClearValue clearDepth = vk::ClearDepthStencilValue{
                    .depth = 1.0f,
                    .stencil = 0
            };
            for(const auto& r : regions) {
                data.elements.emplace_back(r);
                data.elements.back().toDraw.colorOutput = builder.read(r.toDraw.colorOutput, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.elements.back().toDraw.depthStencil = builder.read(r.toDraw.depthStencil, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
                data.elements.back().discardWhereDepthIsUnwritten = r.discardWhereDepthIsUnwritten;
            }
            data.color = builder.createRenderTarget("Composed color",
                                                    vk::Format::eR8G8B8A8Unorm,
                                                   {},
                                                   vk::AttachmentLoadOp::eClear,
                                                   clearColor,
                                                   vk::ImageLayout::eColorAttachmentOptimal);

            data.depthStencil = builder.createRenderTarget("Composed depth stencil",
                                                           GetVulkanDriver().getDepthFormat(),
                                                           {},
                                                           vk::AttachmentLoadOp::eClear,
                                                           clearDepth,
                                                           vk::ImageLayout::eDepthStencilAttachmentOptimal);
        },
        [](const Render::CompiledPass& pass, const Render::Context& frame, const PassData::Composer& data, vk::CommandBuffer& cmds) {
            ZoneScopedN("CPU RenderGraph Composer");

            //TODO; // STENCIL STATE to reset
            cmds.setStencilTestEnable(false);
            auto& renderer = frame.renderer;
            auto pipeline = renderer.getOrCreateRenderPassSpecificPipeline("composer-blit", pass);
            auto& screenQuad = renderer.getFullscreenQuad();
            screenQuad.bind(cmds);

            std::uint32_t index = 0;
            for(const auto& e : data.elements) {
                auto& colorTexture = pass.getGraph().getTexture(e.toDraw.colorOutput, frame.frameNumber);
                auto& depthStencilTexture = pass.getGraph().getTexture(e.toDraw.depthStencil, frame.frameNumber);
                renderer.bindTexture(*pipeline, frame, colorTexture, 0, 1, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, index);
                renderer.bindTexture(*pipeline, frame, depthStencilTexture, 0, 2, nullptr, vk::ImageAspectFlagBits::eDepth, vk::ImageViewType::e2D, index);
                index++;
            }

            // fill remaining slots
            for (size_t i = index; i < 16 /* TODO: base on constant inside .json file*/ ; i++) {
                renderer.unbindTexture(*pipeline, frame, 0, 1, i);
                renderer.unbindTexture(*pipeline, frame, 0, 2, i);
            }

            index = 0;
            pipeline->bind(pass, frame, cmds);
            for(const auto& e : data.elements) {
                struct Region {
                    float left;
                    float right;
                    float top;
                    float bottom;
                    float depth;
                    std::uint32_t texIndex;
                    std::uint32_t discardWhereDepthIsUnwritten;
                };

                Region r = {
                    .left = e.left,
                    .right = e.right,
                    .top = e.top,
                    .bottom = e.bottom,
                    .depth = e.depth,
                    .texIndex = index,
                    .discardWhereDepthIsUnwritten = e.discardWhereDepthIsUnwritten,
                };
                renderer.pushConstantBlock("region", *pipeline, frame, vk::ShaderStageFlagBits::eVertex, cmds, r);

                screenQuad.draw(cmds);

                index++;
            }
        });
    }

    void Composer::clear() {
        regions.clear();
    }

    bool Composer::hasRegions() const {
        return !regions.empty();
    }

}
