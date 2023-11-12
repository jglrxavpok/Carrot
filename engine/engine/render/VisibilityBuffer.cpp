//
// Created by jglrxavpok on 12/11/2023.
//

#include "VisibilityBuffer.h"
#include <engine/render/VulkanRenderer.h>
#include <engine/utils/Profiling.h>
#include <engine/Engine.h>

namespace Carrot::Render {

    VisibilityBuffer::VisibilityBuffer(VulkanRenderer& renderer): renderer(renderer) {

    }

    struct VisibilityBufferRasterizationData {
        Render::FrameResource visibilityBuffer;
    };

    const VisibilityBuffer::VisibilityPassData& VisibilityBuffer::addVisibilityBufferPasses(Render::GraphBuilder& graph, const Render::PassData::GBuffer& gBufferData, const Render::TextureSize& framebufferSize) {
        // Add the hardware rasterization pass
        auto& rasterizePass = graph.addPass<VisibilityBufferRasterizationData>("rasterize visibility buffer",
            [this, framebufferSize](Render::GraphBuilder& builder, Render::Pass<VisibilityBufferRasterizationData>& pass, VisibilityBufferRasterizationData& data) {
                vk::ClearColorValue clearVisibilityValue{ (std::uint32_t)0, (std::uint32_t)0, (std::uint32_t)0, (std::uint32_t)0 };
                // Declare the visibility buffer texture
                data.visibilityBuffer = builder.createRenderTarget("visibility buffer", vk::Format::eR64Uint, framebufferSize, vk::AttachmentLoadOp::eClear,
                                                                   clearVisibilityValue);
            },
            [this](const Render::CompiledPass& pass, const Render::Context& frame, const VisibilityBufferRasterizationData& data, vk::CommandBuffer& cmds) {
                ZoneScopedN("CPU RenderGraph visibility buffer rasterize");
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "visibility buffer rasterize");
                // Record all render packets (~= draw commands) with the tag "VisibilityBuffer"
                frame.renderer.recordPassPackets(Render::PassEnum::VisibilityBuffer, pass.getRenderPass(), frame, cmds);
            }
        );

        // Take visibility buffer as input and render materials to GBuffer
        auto& materialPass = graph.addPass<VisibilityPassData>("visibility buffer materials",
            [this, &gBufferData, &rasterizePass](Render::GraphBuilder& builder, Render::Pass<VisibilityPassData>& pass, VisibilityPassData& data) {
                data.gbuffer.writeTo(builder, gBufferData);
                data.visibilityBuffer = builder.read(rasterizePass.getData().visibilityBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
            },
            [this](const Render::CompiledPass& pass, const Render::Context& frame, const VisibilityPassData& data, vk::CommandBuffer& cmds) {
                // no-op for now
            }
        );

        return materialPass.getData();
    }
} // Carrot::Render