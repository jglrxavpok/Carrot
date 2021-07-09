//
// Created by jglrxavpok on 20/06/2021.
//

#include "RenderGraph.h"
#include "engine/utils/Assert.h"
#include "engine/io/Logging.hpp"
#include "engine/utils/Macros.h"

namespace Carrot::Render {
    GraphBuilder::GraphBuilder(VulkanDriver& driver): driver(driver) {
        swapchainImage.format = driver.getSwapchainImageFormat();
        swapchainImage.isSwapchain = true;
        resources.emplace_back(&swapchainImage);
    }

    FrameResource& GraphBuilder::read(const FrameResource& toRead, vk::ImageLayout expectedLayout, vk::ImageAspectFlags aspect) {
        resources.emplace_back(&toRead);
        currentPass->addInput(resources.back(), expectedLayout, aspect);
        textureUsages[toRead.rootID] |= vk::ImageUsageFlagBits::eSampled;
        return resources.back();
    }

    FrameResource& GraphBuilder::write(const FrameResource& toWrite, vk::AttachmentLoadOp loadOp, vk::ImageLayout layout, vk::ImageAspectFlags aspect) {
        return write(toWrite, loadOp, layout, vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f}), aspect);
    }

    FrameResource& GraphBuilder::write(const FrameResource& toWrite, vk::AttachmentLoadOp loadOp, vk::ImageLayout layout, vk::ClearValue clearValue, vk::ImageAspectFlags aspect) {
        resources.emplace_back(&toWrite);
        currentPass->addOutput(resources.back(), loadOp, clearValue, aspect, layout);
        return resources.back();
    }

    void GraphBuilder::present(const FrameResource& resourceToPresent) {
        currentPass->present(resourceToPresent);
    }

    FrameResource& GraphBuilder::createRenderTarget(vk::Format format, TextureSize size, vk::AttachmentLoadOp loadOp,
                                                    vk::ClearValue clearValue, vk::ImageLayout layout) {
        auto& r = resources.emplace_back();
        r.format = format;
        r.size = size;
        r.isSwapchain = false;
        currentPass->finalLayouts[r.id] = layout;

        auto aspect = static_cast<vk::ImageAspectFlags>(0);

        switch (layout) {
            case vk::ImageLayout::eStencilAttachmentOptimal:
            case vk::ImageLayout::eDepthAttachmentOptimal:
            case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                textureUsages[r.rootID] |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
                aspect |= vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
                break;

            case vk::ImageLayout::eGeneral:
                textureUsages[r.rootID] |= vk::ImageUsageFlagBits::eStorage;
                aspect |= vk::ImageAspectFlagBits::eColor;
                break;

            case vk::ImageLayout::eColorAttachmentOptimal:
                textureUsages[r.rootID] |= vk::ImageUsageFlagBits::eColorAttachment;
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
        auto result = std::make_unique<Graph>(driver, textureUsages);

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

    vk::ImageUsageFlags GraphBuilder::getFrameResourceUsages(const FrameResource& resource) const {
        auto it = textureUsages.find(resource.rootID);
        if(it != textureUsages.end()) {
            return it->second;
        }
        return static_cast<vk::ImageUsageFlagBits>(0);
    }

    Graph::Graph(VulkanDriver& driver, std::unordered_map<Carrot::UUID, vk::ImageUsageFlags> textureUsages): driver(driver), textureUsages(std::move(textureUsages)) {

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

    Texture& Graph::getTexture(const Carrot::UUID& id, size_t frameIndex) const {
        auto it = textures[frameIndex].find(id);
        runtimeAssert(it != textures[frameIndex].end(), "Did not create texture correctly?");
        return *it->second;
    }

    Texture& Graph::getTexture(const FrameResource& resource, size_t frameIndex) const {
        return getTexture(resource.rootID, frameIndex);
    }

    Render::Texture& Graph::createTexture(const FrameResource& resource, size_t frameIndex) {
        // TODO: aliasing
        if(textures.empty()) {
            textures.resize(driver.getSwapchainImageCount());
        }

        auto it = textures[frameIndex].find(resource.rootID);
        if(it == textures[frameIndex].end()) {
            vk::Extent3D size;
            auto& swapchainExtent = driver.getSwapchainExtent();
            switch(resource.size.type) {
                case TextureSize::Type::SwapchainProportional: {
                    size.width = static_cast<std::uint32_t>(resource.size.width * swapchainExtent.width);
                    size.height = static_cast<std::uint32_t>(resource.size.height * swapchainExtent.height);
                    size.depth = static_cast<std::uint32_t>(resource.size.depth * 1);
                } break;

                case TextureSize::Type::Fixed: {
                    size.width = static_cast<std::uint32_t>(resource.size.width);
                    size.height = static_cast<std::uint32_t>(resource.size.height);
                    size.depth = static_cast<std::uint32_t>(resource.size.depth);
                } break;
            }
            auto format = resource.format;

            auto usage = textureUsages[resource.rootID];
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
        } else {
            throw std::runtime_error("Texture already exists");
        }

        return getTexture(resource, frameIndex);
    }

    void Graph::onSwapchainImageCountChange(size_t newCount) {
        for(auto& [n, pass] : passes) {
            pass->onSwapchainImageCountChange(newCount);
        }
    }

    void Graph::onSwapchainSizeChange(int newWidth, int newHeight) {
        textures.clear();
        for(auto& [n, pass] : passes) {
            pass->onSwapchainSizeChange(newWidth, newHeight);
        }
    }
}
