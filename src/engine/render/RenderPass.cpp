//
// Created by jglrxavpok on 20/06/2021.
//

#include "RenderPass.h"

#include <utility>
#include <engine/utils/Containers.h>
#include "RenderGraph.h"
#include "engine/utils/Assert.h"
#include "engine/utils/Macros.h"

Carrot::Render::CompiledPass::CompiledPass(
        Carrot::Render::Graph& graph,
        vk::UniqueRenderPass&& renderPass,
        const std::vector<vk::ClearValue>& clearValues,
        const CompiledPassCallback& renderingCode,
        std::vector<ImageTransition>&& prePassTransitions,
        InitCallback initCallback
        ):
        graph(graph),
        framebuffers(),
        renderPass(std::move(renderPass)),
        clearValues(clearValues),
        renderingCode(renderingCode),
        prePassTransitions(prePassTransitions),
        initCallback(std::move(initCallback))
        {
            rasterized = true;
            framebuffers = std::move(this->initCallback(*this));
        }

Carrot::Render::CompiledPass::CompiledPass(
        Carrot::Render::Graph& graph,
        const CompiledPassCallback& renderingCode,
        std::vector<ImageTransition>&& prePassTransitions,
        InitCallback initCallback
        ):
        graph(graph),
        framebuffers(),
        renderPass(),
        renderingCode(renderingCode),
        prePassTransitions(prePassTransitions),
        initCallback(std::move(initCallback))
        {
            rasterized = false;
            auto fb = this->initCallback(*this);
            runtimeAssert(fb.empty(), "No framebuffers should be generated for a non-rasterized pass");
        }

void Carrot::Render::CompiledPass::execute(const Render::Context& data, vk::CommandBuffer& cmds) const {
    for(const auto& transition : prePassTransitions) {
        auto& tex = graph.getTexture(transition.resourceID, data.swapchainIndex);
        tex.assumeLayout(transition.from);
        tex.transitionInline(cmds, transition.to, transition.aspect);
    }

    if(rasterized) {
        cmds.beginRenderPass(vk::RenderPassBeginInfo {
                .renderPass = *renderPass,
                .framebuffer = *framebuffers[data.swapchainIndex],
                .renderArea = {
                        .offset = vk::Offset2D{0, 0},
                        .extent = getVulkanDriver().getSwapchainExtent()
                },
                .clearValueCount = static_cast<uint32_t>(clearValues.size()),
                .pClearValues = clearValues.data(),
        }, vk::SubpassContents::eInline);
        getVulkanDriver().updateViewportAndScissor(cmds);
    }

    renderingCode(*this, data, cmds);

    if(rasterized) {
        cmds.endRenderPass();
    }
}

void Carrot::Render::PassBase::addInput(Carrot::Render::FrameResource& resource, vk::ImageLayout expectedLayout, vk::ImageAspectFlags aspect) {
    inputs.emplace_back(resource, expectedLayout, aspect);
    inputs.back().resource.updateLayout(expectedLayout);
    resource.updateLayout(expectedLayout);
}

void Carrot::Render::PassBase::addOutput(Carrot::Render::FrameResource& resource, vk::AttachmentLoadOp loadOp,
                                         vk::ClearValue clearValue, vk::ImageAspectFlags aspect, vk::ImageLayout layout) {
    outputs.emplace_back(resource, loadOp, clearValue, aspect);
    outputs.back().resource.updateLayout(layout);
    resource.updateLayout(layout);
    finalLayouts[resource.id] = layout;
}

void Carrot::Render::PassBase::present(FrameResource& toPresent) {
    auto output = std::find_if(WHOLE_CONTAINER(outputs), [&](const auto& o) { return o.resource.rootID == toPresent.rootID; });
    runtimeAssert(output != outputs.end(), "Presentable resource must be in outputs of this render pass!");

    // not using updateLayout to keep previousLayout value
    toPresent.layout = vk::ImageLayout::ePresentSrcKHR;
    output->resource.layout = vk::ImageLayout::ePresentSrcKHR;
}

std::unique_ptr<Carrot::Render::CompiledPass> Carrot::Render::PassBase::compile(Carrot::VulkanDriver& driver, Carrot::Render::Graph& graph) {
    // TODO: input and output can be the same attachment (subpasses)
    std::vector<vk::AttachmentDescription> attachments;
    std::vector<vk::AttachmentReference> inputAttachments;
    std::vector<vk::AttachmentReference> outputAttachments;
    std::unique_ptr<vk::AttachmentReference> depthAttachmentRef = nullptr;

    std::vector<ImageTransition> prePassTransitions;
    for(const auto& input : inputs) {
        auto initialLayout = input.resource.previousLayout;//graph.getFinalLayouts()[input.resource.parentID]; // input is not the resource itself, but a new instance returned by read()
        assert(initialLayout != vk::ImageLayout::eUndefined);
        if(initialLayout != input.resource.layout) {
            prePassTransitions.emplace_back(input.resource.parentID, initialLayout, input.resource.layout, input.aspect);
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

            if(output.loadOp == vk::AttachmentLoadOp::eClear) {
                clearValues.push_back(output.clearValue);
            }

            if(finalLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal
               || finalLayout == vk::ImageLayout::eDepthAttachmentOptimal
               || finalLayout == vk::ImageLayout::eStencilAttachmentOptimal) {
                runtimeAssert(!depthAttachmentRef, "Only one depth-stencil is allowed at once.");
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

        auto init = [outputs = outputs](CompiledPass& pass) {
            auto& graph = pass.getGraph();
            auto& driver = pass.getVulkanDriver();
            std::vector<std::vector<vk::ImageView>> attachmentImageViews(driver.getSwapchainImageCount()); // [frameIndex][attachmentIndex], one per swapchain image
            std::vector<vk::UniqueFramebuffer> framebuffers(driver.getSwapchainImageCount());
            for (int i = 0; i < driver.getSwapchainImageCount(); ++i) {
                auto& frameImageViews = attachmentImageViews[i];

                // TODO: inputs
                for (const auto& output : outputs) {
                    auto& texture = graph.createTexture(output.resource, i);
                    frameImageViews.push_back(texture.getView(output.aspect));
                }


                vk::FramebufferCreateInfo framebufferInfo{
                        .renderPass = pass.getRenderPass(),
                        .attachmentCount = static_cast<uint32_t>(frameImageViews.size()),
                        .pAttachments = frameImageViews.data(),
                        .width = driver.getSwapchainExtent().width,
                        .height = driver.getSwapchainExtent().height,
                        .layers = 1,
                };

                framebuffers[i] = std::move(driver.getLogicalDevice().createFramebufferUnique(framebufferInfo,
                                                                                              driver.getAllocationCallbacks()));
            }
            return framebuffers;
        };
        result = std::make_unique<CompiledPass>(graph, std::move(renderPass), clearValues,
                                         generateCallback(), std::move(prePassTransitions), init);
    } else {
        auto init = [outputs = outputs](CompiledPass& pass) {
            for (int i = 0; i < pass.getVulkanDriver().getSwapchainImageCount(); ++i) {
                auto& graph = pass.getGraph();
                for (const auto& output : outputs) {
                    DISCARD(graph.createTexture(output.resource, i)); // force create textures
                }
            }
            return std::vector<vk::UniqueFramebuffer>{}; // no framebuffers for non-rasterized passes
        };
        result = make_unique<CompiledPass>(graph, generateCallback(), std::move(prePassTransitions), init);
    }
    postCompile(*result);
    return result;
}

void Carrot::Render::CompiledPass::onSwapchainImageCountChange(size_t newCount) {

}

void Carrot::Render::CompiledPass::onSwapchainSizeChange(int newWidth, int newHeight) {
    framebuffers.clear();
    framebuffers = std::move(initCallback(*this));
}

Carrot::VulkanDriver& Carrot::Render::CompiledPass::getVulkanDriver() const {
    return graph.getVulkanDriver();
}
