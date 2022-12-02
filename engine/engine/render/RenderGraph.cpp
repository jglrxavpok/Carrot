//
// Created by jglrxavpok on 20/06/2021.
//

#include "RenderGraph.h"
#include "core/utils/Assert.h"
#include "core/io/Logging.hpp"
#include "engine/utils/Macros.h"
#include "engine/render/TextureRepository.h"
#include "engine/vulkan/CustomTracyVulkan.h"
#include "engine/Engine.h"

namespace Carrot::Render {
    GraphBuilder::GraphBuilder(VulkanDriver& driver) {
        swapchainImage.format = GetVulkanDriver().getSwapchainImageFormat();
        swapchainImage.imageOrigin = ImageOrigin::SurfaceSwapchain;
        swapchainImage.owner = this;
        resources.emplace_back(&swapchainImage);
    }

    VulkanDriver& GraphBuilder::getVulkanDriver() {
        return GetVulkanDriver();
    }

    FrameResource& GraphBuilder::read(const FrameResource& toRead, vk::ImageLayout expectedLayout, vk::ImageAspectFlags aspect) {
        assert(currentPass);
        resources.emplace_back(&toRead);
        currentPass->addInput(resources.back(), expectedLayout, aspect);
        switch(expectedLayout) {
            case vk::ImageLayout::eTransferSrcOptimal:
                GetVulkanDriver().getTextureRepository().getUsages(toRead.rootID) |= vk::ImageUsageFlagBits::eTransferSrc;
                break;

            case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
            case vk::ImageLayout::eDepthReadOnlyOptimal:
                GetVulkanDriver().getTextureRepository().getUsages(toRead.rootID) |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
                GetVulkanDriver().getTextureRepository().getUsages(toRead.rootID) |= vk::ImageUsageFlagBits::eSampled;
                break;


            default:
                GetVulkanDriver().getTextureRepository().getUsages(toRead.rootID) |= vk::ImageUsageFlagBits::eSampled;
                break;
        }
        return resources.back();
    }

    FrameResource& GraphBuilder::write(const FrameResource& toWrite, vk::AttachmentLoadOp loadOp, vk::ImageLayout layout, vk::ImageAspectFlags aspect) {
        return write(toWrite, loadOp, layout, vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f}), aspect);
    }

    FrameResource& GraphBuilder::write(const FrameResource& toWrite, vk::AttachmentLoadOp loadOp, vk::ImageLayout layout, vk::ClearValue clearValue, vk::ImageAspectFlags aspect) {
        assert(currentPass);
        if(toWrite.owner != this) {
          //  TODO?
        }
        switch(layout) {
            case vk::ImageLayout::eTransferDstOptimal:
                GetVulkanDriver().getTextureRepository().getUsages(toWrite.rootID) |= vk::ImageUsageFlagBits::eTransferDst;
                break;

            case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            case vk::ImageLayout::eDepthAttachmentOptimal:
            case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
            case vk::ImageLayout::eDepthReadOnlyOptimal:
                GetVulkanDriver().getTextureRepository().getUsages(toWrite.rootID) |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
                break;

            default:
                break;
        }
        resources.emplace_back(&toWrite);
        currentPass->addOutput(resources.back(), loadOp, clearValue, aspect, layout, false);
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
        r.imageOrigin = ImageOrigin::Created;
        r.updateLayout(layout);
        currentPass->finalLayouts[r.id] = layout;

        GetVulkanDriver().getTextureRepository().setCreatorID(r.rootID, currentPass->passID);

        auto aspect = static_cast<vk::ImageAspectFlags>(0);

        switch (layout) {
            case vk::ImageLayout::eStencilAttachmentOptimal:
            case vk::ImageLayout::eDepthAttachmentOptimal:
            case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                GetVulkanDriver().getTextureRepository().getUsages(r.rootID) |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
                aspect |= vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
                break;

            case vk::ImageLayout::eGeneral:
                GetVulkanDriver().getTextureRepository().getUsages(r.rootID) |= vk::ImageUsageFlagBits::eStorage;
                GetVulkanDriver().getTextureRepository().getUsages(r.rootID) |= vk::ImageUsageFlagBits::eColorAttachment;
                aspect |= vk::ImageAspectFlagBits::eColor;
                break;

            case vk::ImageLayout::eColorAttachmentOptimal:
                GetVulkanDriver().getTextureRepository().getUsages(r.rootID) |= vk::ImageUsageFlagBits::eColorAttachment;
                aspect |= vk::ImageAspectFlagBits::eColor;
                break;

            default:
                Carrot::Log::warn("Resource %llu x %llu x %llu of format %llu has layout %llu which is not yet fully supported.", r.size.width, r.size.height, r.size.depth, r.format, layout);
                aspect |= vk::ImageAspectFlagBits::eColor;
                break;
        }

        currentPass->addOutput(r, loadOp, clearValue, aspect, layout, true);
        return r;
    }

    std::unique_ptr<Graph> GraphBuilder::compile() {
        auto result = std::make_unique<Graph>(GetVulkanDriver());

        for(const auto& [name, pass] : passes) {
            result->passes.emplace_back(name, std::move(pass->compile(GetVulkanDriver(), *result)));
        }

        // TODO: actually sort
        for(auto& [name, pass] : result->passes) {
            result->sortedPasses.push_back(pass.get());
        }

        result->passesData = passesData;
        return result;
    }

    Graph::Graph(VulkanDriver& driver): driver(driver) {

    }

    void Graph::execute(const Render::Context& data, vk::CommandBuffer& cmds) {
        ZoneScoped;
        for(auto* pass : sortedPasses) {
#ifdef TRACY_ENABLE
            std::string passName = "Execute pass ";
            GetVulkanDriver().setFormattedMarker(cmds, "Pass %s", pass->getName().data());

            passName += pass->getName();
            ZoneScopedN("Execute pass");
#endif
            pass->execute(data, cmds);
        }
    }

    Texture& Graph::getTexture(const FrameResource& resource, size_t frameIndex) const {
        return driver.getTextureRepository().get(resource, frameIndex);
    }

    Texture& Graph::getTexture(const Carrot::UUID& resourceID, size_t frameIndex) const {
        return driver.getTextureRepository().get(resourceID, frameIndex);
    }

    Render::Texture& Graph::createTexture(const FrameResource& resource, size_t frameIndex, const vk::Extent2D& viewportSize) {
        return driver.getTextureRepository().create(resource, frameIndex, driver.getTextureRepository().getUsages(resource.rootID), viewportSize);
    }

    Render::Texture& Graph::getOrCreateTexture(const FrameResource& resource, size_t frameIndex, const vk::Extent2D& viewportSize) {
        return driver.getTextureRepository().getOrCreate(resource, frameIndex, driver.getTextureRepository().getUsages(resource.rootID), viewportSize);
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

    Render::CompiledPass* Graph::getPass(std::string_view passName) const {
        for(const auto& [name, pass] : passes) {
            if(name == passName) {
                return pass.get();
            }
        }
        return nullptr;
    }
}
