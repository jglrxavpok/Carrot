//
// Created by jglrxavpok on 20/06/2021.
//

#include "RenderGraph.h"
#include "engine/utils/Assert.h"

namespace Carrot::Render {
    GraphBuilder::GraphBuilder(VulkanDriver& driver): driver(driver) {
        swapchainImage.width = driver.getSwapchainExtent().width;
        swapchainImage.height = driver.getSwapchainExtent().height;
        swapchainImage.depth = 1;
        swapchainImage.format = driver.getSwapchainImageFormat();
        swapchainImage.isSwapchain = true;
        resources.emplace_back(&swapchainImage);
    }

    FrameResource& GraphBuilder::read(const FrameResource& toRead, vk::ImageLayout expectedLayout) {
        resources.emplace_back(&toRead);
        currentPass->addInput(resources.back(), expectedLayout);
        return resources.back();
    }

    FrameResource& GraphBuilder::write(const FrameResource& toWrite, vk::AttachmentLoadOp loadOp, vk::ClearValue clearValue) {
        resources.emplace_back(&toWrite);
        currentPass->addOutput(resources.back(), loadOp, clearValue);
        return resources.back();
    }

    void GraphBuilder::present(const FrameResource& resourceToPresent) {
        currentPass->present(resourceToPresent);
    }

    FrameResource& GraphBuilder::createRenderTarget(vk::Format format, vk::Extent3D size, vk::AttachmentLoadOp loadOp,
                                                    vk::ClearValue clearValue) {
        auto& r = resources.emplace_back();
        r.format = format;
        r.width = size.width;
        r.height = size.height;
        r.depth = size.depth;
        r.isSwapchain = false;
        currentPass->addOutput(r, loadOp, clearValue);
        return r;
    }

    std::unique_ptr<Graph> GraphBuilder::compile() {
        auto result = std::make_unique<Graph>();

        result->textures.resize(driver.getSwapchainImageCount());
        for(const auto& [name, pass] : passes) {
            result->passes[name] = std::move(pass->compile(driver, *result));
        }

        // TODO: actually sort
        for(const auto& [name, _] : passes) {
            result->sortedPasses.push_back(result->passes[name].get());
        }
        return result;
    }

    Graph::Graph() {

    }

    void Graph::execute(const FrameData& data, vk::CommandBuffer& cmds) {
        for(const auto* pass : sortedPasses) {
            pass->execute(data, cmds);
        }
    }

    CompiledPass& Graph::getPass(const string& name) {
        auto it = passes.find(name);
        runtimeAssert(it != passes.end(), "Could not find pass with given name");
        return *it->second;
    }

    Texture& Graph::getTexture(const Carrot::UUID& id, size_t frameIndex) const {
        auto it = textures[frameIndex].find(id);
        runtimeAssert(it != textures[frameIndex].end(), "Did not create texture correctly?");
        return *it->second;
    }

    Texture& Graph::getTexture(const FrameResource& resource, size_t frameIndex) const {
        return getTexture(resource.rootID, frameIndex);
    }

    Render::Texture& Graph::getOrCreateTexture(VulkanDriver& driver, const FrameResource& resource, size_t frameIndex) {
        // TODO: aliasing
        if(textures.empty()) {
            textures.resize(driver.getSwapchainImageCount());
        }

        auto it = textures[frameIndex].find(resource.rootID);
        if(it == textures[frameIndex].end()) {
            vk::Extent3D size {
                .width = resource.width,
                .height = resource.height,
                .depth = resource.depth,
            };
            auto format = resource.format;

            // TODO: find correct usages automatically
            auto usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment;
            Texture::Ref texture;

            if(resource.isSwapchain) {
                texture = driver.getSwapchainTextures()[frameIndex];
            } else {
                texture = std::make_shared<Texture>(driver,
                                          size,
                                          usage,
                                          format
                );
            }
            textures[frameIndex][resource.rootID] = std::move(texture);
        }

        return getTexture(resource, frameIndex);
    }
}
