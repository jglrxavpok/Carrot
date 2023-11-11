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
        const vk::Extent2D& viewportSize,
        vk::UniqueRenderPass&& renderPass,
        const std::vector<vk::ClearValue>& clearValues,
        const CompiledPassCallback& renderingCode,
        std::vector<ImageTransition>&& prePassTransitions,
        InitCallback initCallback,
        SwapchainRecreationCallback swapchainCallback,
        bool prerecordable,
        const Carrot::UUID& passID
        ):
        graph(graph),
        framebuffers(),
        renderPass(std::move(renderPass)),
        clearValues(clearValues),
        renderingCode(renderingCode),
        prePassTransitions(prePassTransitions),
        initCallback(std::move(initCallback)),
        swapchainRecreationCallback(std::move(swapchainCallback)),
        name(std::move(name)),
        prerecordable(prerecordable),
        passID(passID)
        {
            rasterized = true;
            this->viewportSize = viewportSize;
            createFramebuffers();
            if(prerecordable) {
                createCommandPool();
                needsRecord.resize(GetEngine().getSwapchainImageCount());
            }
        }

Carrot::Render::CompiledPass::CompiledPass(
        Carrot::Render::Graph& graph,
        std::string name,
        const vk::Extent2D& viewportSize,
        const CompiledPassCallback& renderingCode,
        std::vector<ImageTransition>&& prePassTransitions,
        InitCallback initCallback,
        SwapchainRecreationCallback swapchainCallback,
        bool prerecordable,
        const Carrot::UUID& passID
        ):
        graph(graph),
        framebuffers(),
        renderPass(),
        renderingCode(renderingCode),
        prePassTransitions(prePassTransitions),
        initCallback(std::move(initCallback)),
        swapchainRecreationCallback(std::move(swapchainCallback)),
        name(std::move(name)),
        prerecordable(prerecordable),
        passID(passID)
        {
            rasterized = false;
            this->viewportSize = viewportSize;
            createFramebuffers();
            if(prerecordable) {
                createCommandPool();
                needsRecord.resize(GetEngine().getSwapchainImageCount());
            }
        }

void Carrot::Render::CompiledPass::performTransitions(const Render::Context& renderContext, vk::CommandBuffer& cmds) {
    { // TODO: pre-record
        ZoneScopedN("Pre-Pass Layout transitions");

        // TODO: batch transitions
        for(const auto& transition : prePassTransitions) {
            auto& tex = graph.getTexture(transition.resourceID, renderContext.swapchainIndex);
            tex.assumeLayout(transition.from);
            tex.transitionInline(cmds, transition.to, transition.aspect);
        }
    }
}

void Carrot::Render::CompiledPass::execute(const Render::Context& renderContext, vk::CommandBuffer& cmds) {
    // TODO: allow whole execute to be pre-recorded?

    performTransitions(renderContext, cmds);

    {
        ZoneScopedN("Render pass recording");

        if(rasterized) {
            cmds.beginRenderPass(vk::RenderPassBeginInfo {
                    .renderPass = *renderPass,
                    .framebuffer = *framebuffers[renderContext.swapchainIndex],
                    .renderArea = {
                            .offset = vk::Offset2D{0, 0},
                            .extent = renderSize,
                    },
                    .clearValueCount = static_cast<uint32_t>(clearValues.size()),
                    .pClearValues = clearValues.data(),
            }, prerecordable ? vk::SubpassContents::eSecondaryCommandBuffers : vk::SubpassContents::eInline);
            if(!prerecordable) {
                getVulkanDriver().updateViewportAndScissor(cmds, renderSize);
            }
        }

        if(prerecordable) {
            if(commandBuffers.empty()) {
                createCommandBuffers(renderContext);
            }

            if(needsRecord[renderContext.swapchainIndex]) {
              //  recordCommands(renderContext);
                needsRecord[renderContext.swapchainIndex] = false;
            }

            cmds.executeCommands(commandBuffers[renderContext.swapchainIndex]);
        } else {
            renderingCode(*this, renderContext, cmds);
        }

        if(rasterized) {
            cmds.endRenderPass();
        }
    }
}

void Carrot::Render::CompiledPass::refresh() {
    std::fill(needsRecord.begin(), needsRecord.end(), true);
}

void Carrot::Render::PassBase::addInput(Carrot::Render::FrameResource& resource, vk::ImageLayout expectedLayout, vk::ImageAspectFlags aspect) {
    inputs.emplace_back(resource, expectedLayout, aspect);
    inputs.back().resource.updateLayout(expectedLayout);
    resource.updateLayout(expectedLayout);
}

void Carrot::Render::PassBase::addOutput(Carrot::Render::FrameResource& resource, vk::AttachmentLoadOp loadOp,
                                         vk::ClearValue clearValue, vk::ImageAspectFlags aspect, vk::ImageLayout layout, bool isCreatedInThisPass) {
    outputs.emplace_back(resource, loadOp, clearValue, aspect);
    outputs.back().resource.updateLayout(layout);
    outputs.back().isCreatedInThisPass = isCreatedInThisPass;
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
    for(const auto& input : inputs) {
        auto initialLayout = input.resource.previousLayout;//graph.getFinalLayouts()[input.resource.parentID]; // input is not the resource itself, but a new instance returned by read()
        //assert(initialLayout != vk::ImageLayout::eUndefined);
        if(initialLayout != input.expectedLayout) {
            auto it = std::find_if(WHOLE_CONTAINER(prePassTransitions), [&](const auto& e) { return e.resourceID == input.resource.rootID; });
            if(it == prePassTransitions.end()) {
                prePassTransitions.emplace_back(input.resource.rootID, initialLayout, input.resource.layout, input.aspect);
            }
        }
    }

    std::vector<vk::ClearValue> clearValues;
    for(const auto& output : outputs) {

        auto initialLayout = vk::ImageLayout::eUndefined;
        auto finalLayout = output.resource.layout;//finalLayouts[output.resource.id];
        assert(finalLayout != vk::ImageLayout::eUndefined);

        /*if(output.resource.id != output.resource.parentID) */{
            initialLayout = output.resource.previousLayout;//graph.getFinalLayouts()[output.resource.parentID];
        }

        if(rasterized) {
            vk::AttachmentDescription attachment {
                    .format = output.resource.format,
                    .samples = vk::SampleCountFlagBits::e1,

                    .loadOp = output.loadOp,
                    .storeOp = vk::AttachmentStoreOp::eStore,

                    .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,

                    .initialLayout = initialLayout,
                    .finalLayout = finalLayout,
            };
            attachments.push_back(attachment);

            /*if(output.loadOp == vk::AttachmentLoadOp::eClear) */{
                clearValues.push_back(output.clearValue);
            }

            if(finalLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal
               || finalLayout == vk::ImageLayout::eDepthAttachmentOptimal
               || finalLayout == vk::ImageLayout::eStencilAttachmentOptimal
               || finalLayout == vk::ImageLayout::eDepthStencilReadOnlyOptimal
               || finalLayout == vk::ImageLayout::eDepthReadOnlyOptimal
               || finalLayout == vk::ImageLayout::eStencilReadOnlyOptimal) {
                verify(!depthAttachmentRef, "Only one depth-stencil is allowed at once.");
                depthAttachmentRef = std::make_unique<vk::AttachmentReference>(vk::AttachmentReference{
                        .attachment = static_cast<uint32_t>(attachments.size()-1),
                        .layout = finalLayout,
                });
            } else {
                if(finalLayout == vk::ImageLayout::ePresentSrcKHR) {
                    finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
                }
                outputAttachments.push_back(vk::AttachmentReference {
                        .attachment = static_cast<uint32_t>(attachments.size()-1),
                        .layout = finalLayout,
                });
            }
        }
    }

    vk::Extent2D viewportSize = window.getFramebufferExtent();
    std::unique_ptr<CompiledPass> result = nullptr;
    if(rasterized) {
        vk::SubpassDescription mainSubpass{
                .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                .inputAttachmentCount = static_cast<uint32_t>(inputAttachments.size()),

                .colorAttachmentCount = static_cast<uint32_t>(outputAttachments.size()),
                // index in this array is used by `layout(location = 0)` inside shaders
                .pColorAttachments = outputAttachments.data(),
                .pDepthStencilAttachment = depthAttachmentRef.get(),

                .preserveAttachmentCount = 0,
        };

        vk::SubpassDependency mainDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,

                .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                vk::PipelineStageFlagBits::eEarlyFragmentTests,
                // TODO: .srcAccessMask = 0,

                .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                vk::PipelineStageFlagBits::eEarlyFragmentTests,
                .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                                 vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        };

        vk::UniqueRenderPass renderPass = driver.getLogicalDevice().createRenderPassUnique(vk::RenderPassCreateInfo{
                .attachmentCount = static_cast<std::uint32_t>(attachments.size()),
                .pAttachments = attachments.data(),

                // TODO: merge compatible render passes into subpasses
                .subpassCount = 1,
                .pSubpasses = &mainSubpass,
                .dependencyCount = 1,
                .pDependencies = &mainDependency,
        });
        DebugNameable::nameSingle(this->name, *renderPass);

        auto init = [outputs = outputs](CompiledPass& pass, const vk::Extent2D& viewportSize, vk::Extent2D& renderSize) {
            auto& graph = pass.getGraph();
            auto& driver = pass.getVulkanDriver();
            std::vector<std::vector<vk::ImageView>> attachmentImageViews(driver.getSwapchainImageCount()); // [frameIndex][attachmentIndex], one per swapchain image
            std::vector<vk::UniqueFramebuffer> framebuffers(driver.getSwapchainImageCount());

            std::uint32_t maxWidth = 0;
            std::uint32_t maxHeight = 0;
            for (int i = 0; i < driver.getSwapchainImageCount(); ++i) {
                auto& frameImageViews = attachmentImageViews[i];

                // TODO: inputs
                for (const auto& output : outputs) {
                    auto& texture = graph.getOrCreateTexture(output.resource, i, viewportSize);
                    frameImageViews.push_back(texture.getView(output.aspect));

                    maxWidth = std::max(texture.getSize().width, maxWidth);
                    maxHeight = std::max(texture.getSize().height, maxHeight);
                }

                Carrot::Log::info("Creating framebuffer of size %lux%lu", maxWidth, maxHeight);


                vk::FramebufferCreateInfo framebufferInfo{
                        .renderPass = pass.getRenderPass(),
                        .attachmentCount = static_cast<uint32_t>(frameImageViews.size()),
                        .pAttachments = frameImageViews.data(),
                        .width = maxWidth,
                        .height = maxHeight,
                        .layers = 1,
                };

                framebuffers[i] = std::move(driver.getLogicalDevice().createFramebufferUnique(framebufferInfo,
                                                                                              driver.getAllocationCallbacks()));
            }
            renderSize.width = maxWidth;
            renderSize.height = maxHeight;
            return framebuffers;
        };
        result = std::make_unique<CompiledPass>(graph, name, viewportSize, std::move(renderPass), clearValues,
                                         generateCallback(), std::move(prePassTransitions), init, generateSwapchainCallback(), prerecordable, passID);
    } else {
        auto init = [outputs = outputs](CompiledPass& pass, const vk::Extent2D& viewportSize, vk::Extent2D& renderSize) {
            for (int i = 0; i < pass.getVulkanDriver().getSwapchainImageCount(); ++i) {
                auto& graph = pass.getGraph();
                for (const auto& output : outputs) {
                    DISCARD(graph.getOrCreateTexture(output.resource, i, viewportSize)); // force create textures
                }
            }
            // no render pass = no render size
            renderSize.width = 0;
            renderSize.height = 0;
            return std::vector<vk::UniqueFramebuffer>{}; // no framebuffers for non-rasterized passes
        };
        result = make_unique<CompiledPass>(graph, name, viewportSize, generateCallback(), std::move(prePassTransitions), init, generateSwapchainCallback(), prerecordable, passID);
    }
    result->setInputsOutputsForDebug(inputs, outputs);
    postCompile(*result);
    return result;
}

void Carrot::Render::CompiledPass::setInputsOutputsForDebug(const std::list<Input>& _inputs, const std::list<Output>& _outputs) {
    inputs.reserve(_inputs.size());
    outputs.reserve(_outputs.size());

    for(const auto& input : _inputs) {
        inputs.emplace_back(input.resource);
    }
    for(const auto& output : _outputs) {
        outputs.emplace_back(output.resource);
    }
    for(const auto& output : _outputs) {
        if(!output.isCreatedInThisPass) {
            inouts.emplace_back(output.resource);
        }
    }
}

void Carrot::Render::CompiledPass::createFramebuffers(){
    framebuffers.clear();
    auto fb = this->initCallback(*this, viewportSize, renderSize);
    verify(rasterized || fb.empty(), "No framebuffers should be generated for a non-rasterized pass");
    framebuffers = std::move(fb);
    swapchainRecreationCallback(*this);
}

void Carrot::Render::CompiledPass::onSwapchainImageCountChange(size_t newCount) {

}

void Carrot::Render::CompiledPass::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) {
    viewportSize = vk::Extent2D {
            .width = static_cast<std::uint32_t>(newWidth),
            .height = static_cast<std::uint32_t>(newHeight),
    };
    getVulkanDriver().getTextureRepository().removeBelongingTo(passID);
    createFramebuffers();
    if(!commandBuffers.empty()) {
        getVulkanDriver().getLogicalDevice().resetCommandPool(*commandPool);
        commandBuffers.clear();
    }
}

Carrot::VulkanDriver& Carrot::Render::CompiledPass::getVulkanDriver() const {
    return graph.getVulkanDriver();
}

void Carrot::Render::CompiledPass::createCommandPool() {
    commandPool = getVulkanDriver().getLogicalDevice().createCommandPoolUnique(vk::CommandPoolCreateInfo {
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = getVulkanDriver().getQueueFamilies().graphicsFamily.value(),
    });
}

void Carrot::Render::CompiledPass::recordCommands(const Carrot::Render::Context& renderContext) {
    auto& cmds = commandBuffers[renderContext.swapchainIndex];
    vk::CommandBufferInheritanceInfo inheritanceInfo {
            .renderPass = *renderPass,
            .subpass = 0, // TODO: modify if subpasses become supported
            .framebuffer = *framebuffers[renderContext.swapchainIndex],
    };
    cmds.begin(vk::CommandBufferBeginInfo {
            .flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse,
            .pInheritanceInfo = &inheritanceInfo,
    });

    if(rasterized) {
        getVulkanDriver().updateViewportAndScissor(cmds, renderSize);
    }

    renderingCode(*this, renderContext, cmds);
    cmds.end();
}

void Carrot::Render::CompiledPass::createCommandBuffers(const Render::Context& renderContext) {
    if(!prerecordable)
        return;
    commandBuffers = getVulkanDriver().getLogicalDevice().allocateCommandBuffers(vk::CommandBufferAllocateInfo {
            .commandPool = *commandPool,
            .level = vk::CommandBufferLevel::eSecondary,
            .commandBufferCount = static_cast<std::uint32_t>(getVulkanDriver().getSwapchainImageCount()),
    });

    for (int i = 0; i < commandBuffers.size(); i++) {
        recordCommands(GetEngine().newRenderContext(i, *renderContext.pViewport, renderContext.eye));
    }
}

void Carrot::Render::FrameResource::updateLayout(vk::ImageLayout newLayout) {
    if(newLayout == layout)
        return;
    previousLayout = layout;
    layout = newLayout;
}