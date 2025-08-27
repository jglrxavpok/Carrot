//
// Created by jglrxavpok on 20/06/2021.
//

#include "RenderPass.h"

#include <utility>
#include <core/utils/Containers.h>
#include "RenderGraph.h"
#include "core/utils/Assert.h"
#include "engine/utils/Macros.h"
#include "core/io/Logging.hpp"
#include "engine/Engine.h"
#include "engine/render/TextureRepository.h"

Carrot::Render::CompiledPass::CompiledPass(
        Carrot::Render::Graph& graph,
        std::string name,
        Vector<Output> _colorAttachments,
        std::optional<Output> _depthAttachment,
        std::optional<Output> _stencilAttachment,
        const vk::Extent2D& viewportSize,
        const CompiledPassCallback& renderingCode,
        std::vector<ImageTransition>&& prePassTransitions,
        InitCallback initCallback,
        SwapchainRecreationCallback swapchainCallback,
        const Carrot::UUID& passID
        ):
        graph(graph),
        renderingCode(renderingCode),
        colorAttachments(std::move(_colorAttachments)),
        depthAttachment(std::move(_depthAttachment)),
        stencilAttachment(std::move(_stencilAttachment)),
        prePassTransitions(prePassTransitions),
        initCallback(std::move(initCallback)),
        swapchainRecreationCallback(std::move(swapchainCallback)),
        name(std::move(name)),
        passID(passID)
        {
            pipelineCreateInfo.colorAttachments.resize(colorAttachments.size());
            for (i32 index = 0; index < colorAttachments.size(); index++) {
                pipelineCreateInfo.colorAttachments[index] = colorAttachments[index].resource.format;
            }
            if (depthAttachment.has_value()) {
                pipelineCreateInfo.depthFormat = depthAttachment->resource.format;
            }
            if (stencilAttachment.has_value()) {
                pipelineCreateInfo.stencilFormat = stencilAttachment->resource.format;
            }

            rasterized = true;
            this->viewportSize = viewportSize;
            recreateResources();
        }

Carrot::Render::CompiledPass::CompiledPass(
        Carrot::Render::Graph& graph,
        std::string name,
        const vk::Extent2D& viewportSize,
        const CompiledPassCallback& renderingCode,
        std::vector<ImageTransition>&& prePassTransitions,
        InitCallback initCallback,
        SwapchainRecreationCallback swapchainCallback,
        const Carrot::UUID& passID
        ):
        graph(graph),
        renderingCode(renderingCode),
        prePassTransitions(prePassTransitions),
        initCallback(std::move(initCallback)),
        swapchainRecreationCallback(std::move(swapchainCallback)),
        name(std::move(name)),
        passID(passID)
        {
            rasterized = false;
            this->viewportSize = viewportSize;
            recreateResources();
        }

void Carrot::Render::CompiledPass::performTransitions(const Render::Context& renderContext, vk::CommandBuffer& cmds) {
    {
        ZoneScopedN("Pre-Pass Layout transitions");

        // TODO: batch transitions
        for(const auto& transition : prePassTransitions) {
            Texture* pTexture = nullptr;
            if (transition.resource.imageOrigin == ImageOrigin::SurfaceSwapchain) {
                pTexture = &getGraph().getSwapchainTexture(transition.resource, renderContext.swapchainImageIndex);
            } else {
                pTexture = &getGraph().getOrCreateTexture(transition.resource, renderContext.frameNumber, viewportSize);
            }
            pTexture->assumeLayout(transition.from);
            pTexture->transitionInline(cmds, transition.to, transition.aspect);
        }

        Carrot::Vector<vk::BufferMemoryBarrier> bufferBarriers;
        for(std::size_t i = 0; i < outputs.size(); i++) {
            if(needBufferClearEachFrame[i]) {
                auto& buffer = graph.getBuffer(outputs[i], renderContext.frameNumber);
                cmds.fillBuffer(buffer.view.getVulkanBuffer(), buffer.view.getStart(), buffer.view.getSize(), 0);
                bufferBarriers.emplaceBack(vk::BufferMemoryBarrier {
                    .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                    .dstAccessMask = vk::AccessFlagBits::eMemoryRead,
                    .buffer = buffer.view.getVulkanBuffer(),
                    .offset = buffer.view.getStart(),
                    .size = buffer.view.getSize(),
                });
            } else if (outputs[i].type == ResourceType::StorageBuffer) {
                auto& buffer = graph.getBuffer(outputs[i], renderContext.frameNumber);
                bufferBarriers.emplaceBack(vk::BufferMemoryBarrier {
                    .srcAccessMask = vk::AccessFlagBits::eMemoryWrite,
                    .dstAccessMask = vk::AccessFlagBits::eMemoryRead,
                    .buffer = buffer.view.getVulkanBuffer(),
                    .offset = buffer.view.getStart(),
                    .size = buffer.view.getSize(),
                });
            }
        }

        if (!bufferBarriers.empty()) {
            cmds.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, static_cast<vk::DependencyFlags>(0), {},
                bufferBarriers, {});
        }
    }
}

const Carrot::RenderingPipelineCreateInfo& Carrot::Render::CompiledPass::getPipelineCreateInfo() const {
    return pipelineCreateInfo;
}

void Carrot::Render::CompiledPass::execute(const Render::Context& renderContext, vk::CommandBuffer& cmds) {
    // TODO: allow whole execute to be pre-recorded?

    performTransitions(renderContext, cmds);

    {
        ZoneScopedN("Render pass recording");

        if(rasterized) {
            // to allow reuse between calls
            thread_local StackAllocator allocator{Allocator::getDefault()};
            allocator.clear();

            auto convert = [&](vk::RenderingAttachmentInfo& output, const Output& passOutput, vk::ImageAspectFlags aspect) {
                Texture* pTexture = nullptr;
                if (passOutput.resource.imageOrigin == ImageOrigin::SurfaceSwapchain) {
                    pTexture = &getGraph().getSwapchainTexture(passOutput.resource, renderContext.swapchainImageIndex);
                } else {
                    pTexture = &getGraph().getOrCreateTexture(passOutput.resource, renderContext.frameNumber, viewportSize);
                }
                output.imageView = pTexture->getView(aspect);
                output.imageLayout = passOutput.resource.layout;
                output.resolveMode = vk::ResolveModeFlagBits::eNone;
                output.loadOp = passOutput.loadOp;
                output.storeOp = vk::AttachmentStoreOp::eStore;
                output.clearValue = passOutput.clearValue;
            };

            Vector<vk::RenderingAttachmentInfo> vkColorAttachments{allocator};
            vkColorAttachments.ensureReserve(colorAttachments.size());
            for (const Output& passOutput : colorAttachments) {
                convert(vkColorAttachments.emplaceBack(), passOutput, vk::ImageAspectFlagBits::eColor);
            }
            vk::RenderingAttachmentInfo vkDepthAttachment;
            vk::RenderingAttachmentInfo vkStencilAttachment;
            const bool hasDepthAttachment = depthAttachment.has_value();
            bool hasStencilAttachment = stencilAttachment.has_value();
            if (hasDepthAttachment && !hasStencilAttachment) {
                convert(vkDepthAttachment, depthAttachment.value(), vk::ImageAspectFlagBits::eDepth);
            }
            if (hasStencilAttachment && !hasDepthAttachment) {
                convert(vkStencilAttachment, stencilAttachment.value(), vk::ImageAspectFlagBits::eStencil);
            }

            if (hasDepthAttachment && hasStencilAttachment) {
                convert(vkDepthAttachment, depthAttachment.value(), vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
                vkStencilAttachment = vkDepthAttachment;
            }

            cmds.beginRendering(vk::RenderingInfo {
                .renderArea = {
                    .offset = vk::Offset2D{0, 0},
                    .extent = renderSize,
                },
                .layerCount = 1,
                .colorAttachmentCount = (u32)vkColorAttachments.size(),
                .pColorAttachments = vkColorAttachments.data(),
                .pDepthAttachment = hasDepthAttachment ? &vkDepthAttachment : nullptr,
                .pStencilAttachment = hasStencilAttachment ? &vkStencilAttachment : nullptr,
            });
            getVulkanDriver().updateViewportAndScissor(cmds, renderSize);
        }

        renderingCode(*this, renderContext, cmds);

        if(rasterized) {
            cmds.endRendering();
        }
    }
}

void Carrot::Render::PassBase::addInput(Carrot::Render::FrameResource& resource, vk::ImageLayout expectedLayout, vk::ImageAspectFlags aspect) {
    inputs.emplace_back(resource, expectedLayout, aspect);
    inputs.back().resource.updateLayout(expectedLayout);
    resource.updateLayout(expectedLayout);
}

void Carrot::Render::PassBase::addOutput(Carrot::Render::FrameResource& resource, vk::AttachmentLoadOp loadOp,
                                         vk::ClearValue clearValue, vk::ImageAspectFlags aspect, vk::ImageLayout layout, bool isCreatedInThisPass, bool clearBufferEachFrame) {
    outputs.emplace_back(resource, layout, loadOp, clearValue, aspect);
    outputs.back().resource.updateLayout(layout);
    outputs.back().isCreatedInThisPass = isCreatedInThisPass;
    outputs.back().clearBufferEachFrame = clearBufferEachFrame;
    resource.updateLayout(layout);
    finalLayouts[resource.id] = layout;
}

void Carrot::Render::PassBase::present(FrameResource& toPresent) {
    auto output = std::find_if(WHOLE_CONTAINER(outputs), [&](const auto& o) { return o.resource.rootID == toPresent.rootID; });
    verify(output != outputs.end(), "Presentable resource must be in outputs of this render pass!");

    // not using updateLayout to keep previousLayout value
    toPresent.layout = vk::ImageLayout::ePresentSrcKHR;
    output->resource.layout = vk::ImageLayout::ePresentSrcKHR;
}

std::unique_ptr<Carrot::Render::CompiledPass> Carrot::Render::PassBase::compile(Carrot::VulkanDriver& driver, Carrot::Window& window, Carrot::Render::Graph& graph) {
    // TODO: input and output can be the same attachment (subpasses)
    std::vector<vk::AttachmentDescription> attachments;
    std::vector<vk::AttachmentReference> inputAttachments;
    std::vector<vk::AttachmentReference> outputAttachments;
    std::unique_ptr<vk::AttachmentReference> depthAttachmentRef = nullptr;

    std::vector<ImageTransition> prePassTransitions;
    for (const auto& input : inputs) {
        auto initialLayout = input.resource.previousLayout;
        if(initialLayout != input.expectedLayout) {
            auto it = std::find_if(WHOLE_CONTAINER(prePassTransitions), [&](const auto& e) { return e.resource.rootID == input.resource.rootID; });
            if(it == prePassTransitions.end()) {
                prePassTransitions.emplace_back(input.resource, initialLayout, input.expectedLayout, input.aspect);
            }
        }
    }

    // with dynamic rendering, the layout transition must be done manually
    for (const auto& output : outputs) {
        auto initialLayout = output.resource.previousLayout;
        if(initialLayout != output.expectedLayout) {
            auto it = std::find_if(WHOLE_CONTAINER(prePassTransitions), [&](const auto& e) { return e.resource.rootID == output.resource.rootID; });
            if(it == prePassTransitions.end()) {
                prePassTransitions.emplace_back(output.resource, initialLayout, output.expectedLayout, output.aspect);
            }
        }
    }

    vk::Extent2D viewportSize = window.getFramebufferExtent();
    std::unique_ptr<CompiledPass> result = nullptr;

    auto resetBuffers = [outputs = outputs, &graph, &driver]() {
        Carrot::Vector<FrameResource> buffersToClear;
        for(const auto& output : outputs) {
            if(output.resource.type != ResourceType::StorageBuffer) {
                continue;
            }

            if(output.isCreatedInThisPass) {
                buffersToClear.emplaceBack(output.resource);
            }
        }

        if(!buffersToClear.empty()) {
            Carrot::Log::debug("clear buffers");
            Carrot::Log::flush();
            driver.performSingleTimeGraphicsCommands([&](vk::CommandBuffer& cmds) {
                for(std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                    for(const auto& b : buffersToClear) {
                        auto& buffer = graph.getBuffer(b, i);
                        cmds.fillBuffer(buffer.view.getVulkanBuffer(), buffer.view.getStart(), buffer.view.getSize(), 0);
                    }
                }
            });
        }
    };
    if(rasterized) {
        Carrot::Vector<Output> colorAttachments;
        std::optional<Output> depthAttachment;
        std::optional<Output> stencilAttachment;
        for (const auto& output : outputs) {
            if (output.resource.type == ResourceType::RenderTarget) {
                if (output.aspect & vk::ImageAspectFlagBits::eColor) {
                    colorAttachments.emplaceBack(output);
                }
                if (output.aspect & vk::ImageAspectFlagBits::eDepth) {
                    verify(!depthAttachment.has_value(), "Only a single depth attachment is allowed");
                    depthAttachment = output;
                }
                if (output.aspect & vk::ImageAspectFlagBits::eStencil) {
                    verify(!stencilAttachment.has_value(), "Only a single stencil attachment is allowed");
                    stencilAttachment = output;
                }
            }
        }
        auto init = [resetBuffers, outputs = outputs](CompiledPass& pass, const vk::Extent2D& viewportSize, vk::Extent2D& renderSize) {
            auto& graph = pass.getGraph();
            auto& driver = pass.getVulkanDriver();

            std::uint32_t maxWidth = 0;
            std::uint32_t maxHeight = 0;
            for (const auto& output : outputs) {
                bool hasOnlyStorageBuffers = true;
                for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                    if(output.resource.type == ResourceType::StorageBuffer) {
                        DISCARD(graph.getOrCreateBuffer(output.resource, i));
                    } else {
                        hasOnlyStorageBuffers = false;
                        if (output.resource.imageOrigin == ImageOrigin::SurfaceSwapchain) {
                            maxWidth = std::max(viewportSize.width, maxWidth);
                            maxHeight = std::max(viewportSize.height, maxHeight);
                        } else {
                            auto& texture = graph.getOrCreateTexture(output.resource, i, viewportSize);
                            maxWidth = std::max(texture.getSize().width, maxWidth);
                            maxHeight = std::max(texture.getSize().height, maxHeight);
                        }
                    }
                }

                verify(!hasOnlyStorageBuffers, "Pass has no render targets nor storage images, are you sure you want rasterization?");
            }
            Carrot::Log::info("Creating framebuffer of size %lux%lu (pass %s)", maxWidth, maxHeight, pass.getName().data());
            resetBuffers();
            renderSize.width = maxWidth;
            renderSize.height = maxHeight;
        };
        result = std::make_unique<CompiledPass>(graph, name, std::move(colorAttachments), std::move(depthAttachment), std::move(stencilAttachment), viewportSize,
                                         generateCallback(), std::move(prePassTransitions), init, generateSwapchainCallback(), passID);
    } else {
        auto init = [resetBuffers, outputs = outputs](CompiledPass& pass, const vk::Extent2D& viewportSize, vk::Extent2D& renderSize) {
            for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                auto& graph = pass.getGraph();
                for (const auto& output : outputs) {
                    // force create resources
                    if(output.resource.type == ResourceType::StorageBuffer) {
                        DISCARD(graph.getOrCreateBuffer(output.resource, i));
                    } else {
                        DISCARD(graph.getOrCreateTexture(output.resource, i, viewportSize));
                    }
                }
            }
            resetBuffers();
            // no render pass = no render size
            renderSize.width = 0;
            renderSize.height = 0;
        };
        result = make_unique<CompiledPass>(graph, name, viewportSize, generateCallback(), std::move(prePassTransitions), init, generateSwapchainCallback(), passID);
    }
    result->setInputsOutputsForDebug(inputs, outputs);
    postCompile(*result);
    return result;
}

void Carrot::Render::CompiledPass::setInputsOutputsForDebug(const std::list<Input>& _inputs, const std::list<Output>& _outputs) {
    inputs.reserve(_inputs.size());
    outputs.reserve(_outputs.size());
    needBufferClearEachFrame.reserve(_outputs.size());

    for(const auto& input : _inputs) {
        inputs.emplace_back(input.resource);
    }
    for(const auto& output : _outputs) {
        outputs.emplace_back(output.resource);

        if(!output.isCreatedInThisPass) {
            inouts.emplace_back(output.resource);
        }
        needBufferClearEachFrame.emplace_back(output.clearBufferEachFrame);
    }
}

void Carrot::Render::CompiledPass::recreateResources(){
    this->initCallback(*this, viewportSize, renderSize);
    swapchainRecreationCallback(*this);
}

void Carrot::Render::CompiledPass::onSwapchainImageCountChange(size_t newCount) {

}

void Carrot::Render::CompiledPass::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) {
    viewportSize = vk::Extent2D {
            .width = static_cast<std::uint32_t>(newWidth),
            .height = static_cast<std::uint32_t>(newHeight),
    };
    getVulkanDriver().getResourceRepository().removeBelongingTo(passID);
    recreateResources();
}

Carrot::VulkanDriver& Carrot::Render::CompiledPass::getVulkanDriver() const {
    return graph.getVulkanDriver();
}

void Carrot::Render::FrameResource::updateLayout(vk::ImageLayout newLayout) {
    previousLayout = layout;
    layout = newLayout;
}