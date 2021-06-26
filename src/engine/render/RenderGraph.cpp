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
    }

    FrameResource& GraphBuilder::read(const FrameResource& toRead) {
        resources.emplace_back(&toRead);
        currentPass->addInput(resources.back());
        return resources.back();
    }

    FrameResource& GraphBuilder::write(const FrameResource& toWrite, vk::AttachmentLoadOp loadOp, vk::ClearValue clearValue) {
        resources.emplace_back(&toWrite);
        currentPass->addOutput(resources.back(), loadOp, clearValue);
        return resources.back();
    }

    std::unique_ptr<Graph> GraphBuilder::compile() {
        auto result = std::make_unique<Graph>();

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
        // TODO
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

    Render::Texture& Graph::getOrCreateTexture(VulkanDriver& driver, const FrameResource& resource, size_t frameIndex) {
        // TODO: proper caching and aliasing
        // TODO: proper allocation
        if(textures.empty()) {
            textures.resize(driver.getSwapchainImageCount());

            /*vk::Extent3D extent {
                .width = resource.width,
                .height = resource.height,
                .depth = resource.depth,
            };
            textures[0] = std::make_unique<Texture>(driver,
                                                    extent,



                    )*/
            for (int i = 0; i < driver.getSwapchainImageCount(); ++i) {
                textures[i].emplace_back(driver.getSwapchainTextures()[i]);
            }
        }
        return *textures[frameIndex][0]; // TODO
    }
}
