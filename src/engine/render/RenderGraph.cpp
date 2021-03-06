//
// Created by jglrxavpok on 20/06/2021.
//

#include "RenderGraph.h"
#include "engine/utils/Assert.h"
#include "engine/io/Logging.hpp"
#include "engine/utils/Macros.h"
#include "engine/render/TextureRepository.h"

namespace Carrot::Render {
    GraphBuilder::GraphBuilder(VulkanDriver& driver): driver(driver) {
        swapchainImage.format = driver.getSwapchainImageFormat();
        swapchainImage.isSwapchain = true;
        swapchainImage.owner = this;
        resources.emplace_back(&swapchainImage);
    }

    FrameResource& GraphBuilder::read(const FrameResource& toRead, vk::ImageLayout expectedLayout, vk::ImageAspectFlags aspect) {
        assert(currentPass);
        resources.emplace_back(&toRead);
        currentPass->addInput(resources.back(), expectedLayout, aspect);
        driver.getTextureRepository().getUsages(toRead.rootID) |= vk::ImageUsageFlagBits::eSampled;
        return resources.back();
    }

    FrameResource& GraphBuilder::write(const FrameResource& toWrite, vk::AttachmentLoadOp loadOp, vk::ImageLayout layout, vk::ImageAspectFlags aspect) {
        return write(toWrite, loadOp, layout, vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f}), aspect);
    }

    FrameResource& GraphBuilder::write(const FrameResource& toWrite, vk::AttachmentLoadOp loadOp, vk::ImageLayout layout, vk::ClearValue clearValue, vk::ImageAspectFlags aspect) {
        assert(currentPass);
        if(toWrite.owner != this) {
            TODO
        }
        resources.emplace_back(&toWrite);
        currentPass->addOutput(resources.back(), loadOp, clearValue, aspect, layout);
        return resources.back();
    }

    void GraphBuilder::present(FrameResource& resourceToPresent) {
        assert(currentPass);
        currentPass->present(resourceToPresent);
    }

    FrameResource& GraphBuilder::createRenderTarget(vk::Format format, TextureSize size, vk::AttachmentLoadOp loadOp,
                                                    vk::ClearValue clearValue, vk::ImageLayout layout) {
        auto& r = resources.emplace_back();
        r.owner = this;
        r.format = format;
        r.size = size;
        r.isSwapchain = false;
        r.updateLayout(layout);
        currentPass->finalLayouts[r.id] = layout;

        auto aspect = static_cast<vk::ImageAspectFlags>(0);

        switch (layout) {
            case vk::ImageLayout::eStencilAttachmentOptimal:
            case vk::ImageLayout::eDepthAttachmentOptimal:
            case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                driver.getTextureRepository().getUsages(r.rootID) |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
                aspect |= vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
                break;

            case vk::ImageLayout::eGeneral:
                driver.getTextureRepository().getUsages(r.rootID) |= vk::ImageUsageFlagBits::eStorage;
                aspect |= vk::ImageAspectFlagBits::eColor;
                break;

            case vk::ImageLayout::eColorAttachmentOptimal:
                driver.getTextureRepository().getUsages(r.rootID) |= vk::ImageUsageFlagBits::eColorAttachment;
                aspect |= vk::ImageAspectFlagBits::eColor;
                break;

            default:
                Carrot::Log::warn("Resource %llu x %llu x %llu of format %llu has layout %llu which is not yet fully supported.", r.size.width, r.size.height, r.size.depth, r.format, layout);
                aspect |= vk::ImageAspectFlagBits::eColor;
                break;
        }

        currentPass->addOutput(r, loadOp, clearValue, aspect, layout);
        return r;
    }

    std::unique_ptr<Graph> GraphBuilder::compile() {
        auto result = std::make_unique<Graph>(driver);

        for(const auto& [name, pass] : passes) {
            result->passes[name] = std::move(pass->compile(driver, *result));
        }

        // TODO: actually sort
        for(const auto& [name, _] : passes) {
            result->sortedPasses.push_back(result->passes[name].get());
        }
        return result;
    }

    Graph::Graph(VulkanDriver& driver): driver(driver) {

    }

    void Graph::execute(const Render::Context& data, vk::CommandBuffer& cmds) {
        for(const auto* pass : sortedPasses) {
            pass->execute(data, cmds);
        }
    }

    CompiledPass& Graph::getPass(const string& name) {
        auto it = passes.find(name);
        runtimeAssert(it != passes.end(), "Could not find pass with given name");
        return *it->second;
    }

    Texture& Graph::getTexture(const FrameResource& resource, size_t frameIndex) const {
        return driver.getTextureRepository().get(resource, frameIndex);
    }

    Texture& Graph::getTexture(const Carrot::UUID& resourceID, size_t frameIndex) const {
        return driver.getTextureRepository().get(resourceID, frameIndex);
    }

    Render::Texture& Graph::createTexture(const FrameResource& resource, size_t frameIndex) {
        return driver.getTextureRepository().create(resource, frameIndex, driver.getTextureRepository().getUsages(resource.rootID));
    }

    void Graph::onSwapchainImageCountChange(size_t newCount) {
        for(auto& [n, pass] : passes) {
            pass->onSwapchainImageCountChange(newCount);
        }
    }

    void Graph::onSwapchainSizeChange(int newWidth, int newHeight) {
        for(auto& [n, pass] : passes) {
            pass->onSwapchainSizeChange(newWidth, newHeight);
        }
    }
}
