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

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_node_editor.h>
#include <imgui_node_editor_internal.h>
#include <engine/console/RuntimeOption.hpp>

namespace ed = ax::NodeEditor;

namespace Carrot::Render {
    static RuntimeOption DebugRenderGraphs("Debug/Render Graphs", false);

    // flags forced on all resources created by RenderGraph
    constexpr vk::ImageUsageFlags ForcedCommonImageFlags{};
    constexpr vk::ImageUsageFlags ForcedRenderTargetFlags = ForcedCommonImageFlags | vk::ImageUsageFlagBits::eSampled;
    constexpr vk::ImageUsageFlags ForcedStorageImageFlags = ForcedCommonImageFlags;
    constexpr vk::ImageUsageFlags ForcedStorageRGBImageFlags = ForcedStorageImageFlags | vk::ImageUsageFlagBits::eSampled;
    constexpr vk::BufferUsageFlags ForcedBufferFlags = vk::BufferUsageFlagBits::eShaderDeviceAddress;

    GraphBuilder::GraphBuilder(VulkanDriver& driver, Window& window): window(window) {
        swapchainImage.format = GetVulkanDriver().getSwapchainImageFormat();
        swapchainImage.imageOrigin = ImageOrigin::SurfaceSwapchain;
        swapchainImage.pOriginWindow = &window;
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
        if(toRead.type != ResourceType::StorageBuffer) {
            switch(expectedLayout) {
                case vk::ImageLayout::eTransferSrcOptimal:
                    GetVulkanDriver().getResourceRepository().getTextureUsages(toRead.rootID) |= vk::ImageUsageFlagBits::eTransferSrc;
                break;

                case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
                case vk::ImageLayout::eDepthReadOnlyOptimal:
                    GetVulkanDriver().getResourceRepository().getTextureUsages(toRead.rootID) |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
                    if (toRead.type != ResourceType::StorageImage) {
                        GetVulkanDriver().getResourceRepository().getTextureUsages(toRead.rootID) |= vk::ImageUsageFlagBits::eSampled;
                    }
                break;


                default:
                    if (toRead.type != ResourceType::StorageImage) {
                        GetVulkanDriver().getResourceRepository().getTextureUsages(toRead.rootID) |= vk::ImageUsageFlagBits::eSampled;
                    }
                break;
            }
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
        if(toWrite.type != ResourceType::StorageBuffer) {
            switch(layout) {
                case vk::ImageLayout::eTransferDstOptimal:
                    GetVulkanDriver().getResourceRepository().getTextureUsages(toWrite.rootID) |= vk::ImageUsageFlagBits::eTransferDst;
                    break;

                case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                case vk::ImageLayout::eDepthAttachmentOptimal:
                case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
                case vk::ImageLayout::eDepthReadOnlyOptimal:
                    GetVulkanDriver().getResourceRepository().getTextureUsages(toWrite.rootID) |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
                    break;

                default:
                    break;
            }
        }
        resources.emplace_back(&toWrite);
        currentPass->addOutput(resources.back(), loadOp, clearValue, aspect, layout, false, false);
        return resources.back();
    }

    void GraphBuilder::present(FrameResource& resourceToPresent) {
        assert(currentPass);
        currentPass->present(resourceToPresent);
    }

    void GraphBuilder::reuseBufferAcrossFrames(const FrameResource& toReuse, std::size_t historyLength) {
        GetVulkanDriver().getResourceRepository().setBufferReuseHistoryLength(toReuse.rootID, historyLength);
    }

    FrameResource& GraphBuilder::createRenderTarget(std::string name, vk::Format format, TextureSize size, vk::AttachmentLoadOp loadOp,
                                              vk::ClearValue clearValue, vk::ImageLayout layout) {
        return createTarget(false, name, format, size, loadOp, clearValue, layout);
    }

    FrameResource& GraphBuilder::createStorageTarget(std::string name, vk::Format format, TextureSize size, vk::ImageLayout layout) {
        return createTarget(true, name, format, size, vk::AttachmentLoadOp::eDontCare, {}, layout);
    }

    FrameResource& GraphBuilder::createBuffer(std::string name, vk::DeviceSize size, vk::BufferUsageFlags usages, bool clearEachFrame) {
        auto& r = resources.emplace_back();
        r.name = std::move(name);
        r.owner = this;
        r.type = ResourceType::StorageBuffer;
        r.bufferSize = size;
        r.imageOrigin = ImageOrigin::Created;

        GetVulkanDriver().getResourceRepository().getBufferUsages(r.rootID) |= usages;
        GetVulkanDriver().getResourceRepository().setCreatorID(r.rootID, currentPass->passID);

        currentPass->addOutput(r, {}, {}, {}, {}, true, clearEachFrame);
        return r;
    }

    FrameResource& GraphBuilder::createTarget(bool isStorageImage, std::string name, vk::Format format, TextureSize size, vk::AttachmentLoadOp loadOp,
                                                    vk::ClearValue clearValue, vk::ImageLayout layout) {
        auto& r = resources.emplace_back();
        r.name = std::move(name);
        r.owner = this;
        r.format = format;
        r.type = isStorageImage ? ResourceType::StorageImage : ResourceType::RenderTarget;
        r.size = size;
        r.imageOrigin = ImageOrigin::Created;
        r.updateLayout(layout);
        currentPass->finalLayouts[r.id] = layout;

        GetVulkanDriver().getResourceRepository().setCreatorID(r.rootID, currentPass->passID);

        auto aspect = static_cast<vk::ImageAspectFlags>(0);

        switch (layout) {
            case vk::ImageLayout::eStencilAttachmentOptimal:
            case vk::ImageLayout::eDepthAttachmentOptimal:
            case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                GetVulkanDriver().getResourceRepository().getTextureUsages(r.rootID) |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
                aspect |= vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
                break;

            case vk::ImageLayout::eGeneral:
                if (!isStorageImage) {
                    GetVulkanDriver().getResourceRepository().getTextureUsages(r.rootID) |= vk::ImageUsageFlagBits::eColorAttachment;
                }
                GetVulkanDriver().getResourceRepository().getTextureUsages(r.rootID) |= vk::ImageUsageFlagBits::eStorage;
                aspect |= vk::ImageAspectFlagBits::eColor;
                break;

            case vk::ImageLayout::eColorAttachmentOptimal:
                GetVulkanDriver().getResourceRepository().getTextureUsages(r.rootID) |= vk::ImageUsageFlagBits::eColorAttachment;
                aspect |= vk::ImageAspectFlagBits::eColor;
                break;

            default:
                Carrot::Log::warn("Resource %llu x %llu x %llu of format %llu has layout %llu which is not yet fully supported.", r.size.width, r.size.height, r.size.depth, r.format, layout);
                aspect |= vk::ImageAspectFlagBits::eColor;
                break;
        }

        currentPass->addOutput(r, loadOp, clearValue, aspect, layout, true, false);
        return r;
    }

    std::unique_ptr<Graph> GraphBuilder::compile() {
        auto result = std::make_unique<Graph>(GetVulkanDriver());

        for(const auto& [name, pass] : passes) {
            result->passes.emplace_back(name, std::move(pass->compile(GetVulkanDriver(), window, *result)));
        }

        // TODO: actually sort
        for(auto& [name, pass] : result->passes) {
            result->sortedPasses.push_back(pass.get());
        }

        result->passesData = passesData;
        return result;
    }

    Graph::Graph(VulkanDriver& driver): driver(driver) {
        ed::Config config;
        nodesContext = ed::CreateEditor(&config);
    }

    static std::uint32_t uniqueID = 1;
    static std::unordered_map<std::uint32_t, ed::NodeId> nodes;

    static ed::NodeId getNodeID(std::uint32_t passIndex) {
        if(nodes.find(passIndex) == nodes.end()) {
            nodes[passIndex] = uniqueID++;
        }
        return nodes[passIndex];
    }
}

struct InputKey {
    std::uint64_t passPtr;
    Carrot::UUID resourceID;

    auto operator<=>(const InputKey&) const = default;
};

template<>
struct std::hash<InputKey> {
    std::size_t operator()(const InputKey& k) const {
        std::size_t hash = 0;
        Carrot::hash_combine(hash, k.passPtr);
        Carrot::hash_combine(hash, k.resourceID.hash());
        return hash;
    }
};

namespace Carrot::Render {
    static std::unordered_map<Carrot::UUID, ed::PinId> inputPinIDs;
    static std::unordered_map<Carrot::UUID, ed::PinId> outputPinIDs;
    static std::unordered_map<Carrot::UUID, std::unordered_map<Carrot::UUID, ed::LinkId>> linkIDs;

    static ed::PinId getInputPinID(const Carrot::UUID& uuid) {
        if(inputPinIDs.find(uuid) == inputPinIDs.end()) {
            inputPinIDs[uuid] = uniqueID++;
        }
        return inputPinIDs[uuid];
    }

    static ed::PinId getOutputPinID(const Carrot::UUID& uuid) {
        if(outputPinIDs.find(uuid) == outputPinIDs.end()) {
            outputPinIDs[uuid] = uniqueID++;
        }
        return outputPinIDs[uuid];
    }

    static ed::LinkId getLinkID(const Carrot::UUID& in, const Carrot::UUID& out) {
        auto& inLinks = linkIDs[in];
        if(inLinks.find(out) == inLinks.end()) {
            inLinks[out] = uniqueID++;
        }
        return inLinks[out];
    }

    void Graph::drawPassNodes(const Render::Context& context, Render::CompiledPass* pass, std::uint32_t passIndex) {
        ed::BeginNode(getNodeID(passIndex));

        ImGui::PushID(passIndex);

        ImGui::BeginVertical("node");
        ImGui::Spring();

        ImGui::Text("%s", std::string(pass->getName()).c_str());

        ImGui::Spring();

        ImGui::BeginHorizontal("content");

        ImGui::BeginVertical("inputs");
        ed::PushStyleVar(ed::StyleVar_PivotAlignment, ImVec2(0, 0.5f));
        ed::PushStyleVar(ed::StyleVar_PivotSize, ImVec2(0, 0));

        auto handlePinMouseInteraction = [&](const FrameResource& o) {
            if(ImGui::IsItemHovered()) {
                hoveredResourceForMain = &o;
                if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    clickedResourceForMain = &o;
                }
            }
        };

        for(const auto& o : pass->getInputOutputs()) {
            ed::BeginPin(getInputPinID(o.id), ed::PinKind::Input);
            ImGui::Text("> %s *", o.name.c_str());
            ed::EndPin();

            handlePinMouseInteraction(o);
        }
        for(const auto& i : pass->getInputs()) {
            ed::BeginPin(getInputPinID(i.id), ed::PinKind::Input);
            ImGui::Text("> %s", i.name.c_str());
            ed::EndPin();

            handlePinMouseInteraction(i);
        }
        ImGui::EndVertical();

        ImGui::BeginVertical("center");
        ImGui::Text("%s", pass->getName().data());
        ImGui::EndVertical();


        ImGui::Spring(1);
        ImGui::BeginVertical("outputs");
        ed::PushStyleVar(ed::StyleVar_PivotAlignment, ImVec2(1.0f, 0.5f));
        ed::PushStyleVar(ed::StyleVar_PivotSize, ImVec2(0, 0));

        for(const auto& o : pass->getOutputs()) {
            ed::BeginPin(getOutputPinID(o.id), ed::PinKind::Output);
            ImGui::Text("%s >", o.name.c_str());
            ed::EndPin();

            handlePinMouseInteraction(o);
        }
        ImGui::EndVertical();

        ImGui::Spring(1);

        ImGui::EndHorizontal();
        ImGui::EndVertical();

        ImGui::PopID();
        ed::EndNode();
    }

    void Graph::debugDraw(const Render::Context& context) {
        std::uint32_t index = 0;

        // draw nodes
        for (auto *pass: sortedPasses) {
            drawPassNodes(context, pass, index++);
        }

        // draw links
        for (auto* pass: sortedPasses) {
            for(const auto& inputSet : { pass->getInputs(), pass->getInputOutputs() }) {
                for(const auto& i : inputSet) {
                    if(i.id != i.parentID) {
                        if(!ed::Link(getLinkID(i.id, i.parentID), getInputPinID(i.id), getOutputPinID(i.parentID))) {
                            ed::Link(getLinkID(i.id, i.parentID), getInputPinID(i.id), getInputPinID(i.parentID));
                        }
                    }
                }
            }
        }
    }

    void Graph::drawResource(const Render::Context& context) {
        if(clickedResourceForMain != nullptr) {
            bool keepOpen = true;
            if(ImGui::Begin("Resource", &keepOpen)) {
                ImGui::Text("%s", clickedResourceForMain->name.c_str());
                if(clickedResourceForMain->type != ResourceType::StorageBuffer) {
                    auto& texture = GetVulkanDriver().getResourceRepository().getTexture(*clickedResourceForMain, context.swapchainIndex);
                    ImGui::Text("%s (%s) (%u x %u x %u)", clickedResourceForMain->id.toString().c_str(), clickedResourceForMain->parentID.toString().c_str(), texture.getSize().width, texture.getSize().height, texture.getSize().depth);
                } else {
                    auto& buffer = GetVulkanDriver().getResourceRepository().getBuffer(*clickedResourceForMain, context.frameCount);
                    ImGui::Text("%s (%s) %llu bytes", clickedResourceForMain->id.toString().c_str(), clickedResourceForMain->parentID.toString().c_str(), buffer.view.getSize());
                }
                float h = ImGui::GetContentRegionAvail().y;

                if(clickedResourceViewer) {
                    float aspectRatio = clickedResourceViewer->getSize().width / (float) clickedResourceViewer->getSize().height;
                    ImGui::Image(clickedResourceViewer->getImguiID(), ImVec2(aspectRatio * h, h));
                }
            }
            ImGui::End();

            if(!keepOpen) {
                clickedResourceForMain = nullptr;
            }
        }

        if(hoveredResourceForMain == nullptr) {
            return;
        }

        ImGui::BeginTooltip();
        ImGui::Text("%s", hoveredResourceForMain->name.c_str());
        if(hoveredResourceForMain->type != ResourceType::StorageBuffer) {
            auto& texture = GetVulkanDriver().getResourceRepository().getTexture(*hoveredResourceForMain, context.swapchainIndex);
            ImGui::Text("%s (%s) (%u x %u x %u)", hoveredResourceForMain->id.toString().c_str(), hoveredResourceForMain->parentID.toString().c_str(), texture.getSize().width, texture.getSize().height, texture.getSize().depth);
        } else {
            auto& buffer = GetVulkanDriver().getResourceRepository().getBuffer(*hoveredResourceForMain, context.frameCount);
            ImGui::Text("%s (%s) %llu bytes", hoveredResourceForMain->id.toString().c_str(), hoveredResourceForMain->parentID.toString().c_str(), buffer.view.getSize());
        }

        if(hoveredResourceViewer) {
            const float h = 512.0f;
            float aspectRatio = hoveredResourceViewer->getSize().width / (float) hoveredResourceViewer->getSize().height;
            ImGui::Image(hoveredResourceViewer->getImguiID(), ImVec2(aspectRatio * h, h));
        }
        ImGui::EndTooltip();
    }

    void Graph::onFrame(const Render::Context& context) {
        static Graph* graphToDebug = nullptr;
        if(DebugRenderGraphs) {
            bool isOpen = true;
            if(ImGui::Begin("Debug render graphs", &isOpen)) {
                ImGui::Separator();

                std::string id = Carrot::sprintf("%x , %llu passes", (std::uint64_t)this, sortedPasses.size());
                if(ImGui::RadioButton(id.c_str(), graphToDebug == this)) {
                    graphToDebug = this;
                }

                if(graphToDebug == this) {
                    ed::SetCurrentEditor((ed::EditorContext*)nodesContext);
                    ed::EnableShortcuts(true);

                    hoveredResourceForMain = nullptr;
                    ed::Begin("Render graph debug");
                    debugDraw(context);
                    ed::End();
                    ed::EnableShortcuts(false);
                }
            }
            ImGui::End();

            if(graphToDebug == this) {
                drawResource(context);
            }

            if(!isOpen) {
                DebugRenderGraphs.setValue(false);
            }
        }
    }

    void Graph::drawViewer(const Render::Context& context, const Render::FrameResource& sourceResource, std::unique_ptr<Texture>& destinationTexture, vk::CommandBuffer cmds) {
        if(!destinationTexture) {
            destinationTexture = std::make_unique<Texture>(driver, vk::Extent3D {
                1024,1024,1
            }, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
            vk::Format::eR8G8B8A8Unorm);
        }

        bool isBuffer = sourceResource.type == ResourceType::StorageBuffer;

        if(isBuffer) {
            auto pipeline = context.renderer.getOrCreatePipelineFullPath("resources/pipelines/compute/debug-buffer-viewer.pipeline", reinterpret_cast<std::uint64_t>(destinationTexture.get()));

            auto& buffer = getBuffer(sourceResource, context.frameCount);

            context.renderer.pushConstants("push", *pipeline, context, vk::ShaderStageFlagBits::eCompute, cmds,
                static_cast<std::uint64_t>(buffer.view.getDeviceAddress()), static_cast<std::uint32_t>(buffer.view.getSize()));
            pipeline->bind({}, context, cmds, vk::PipelineBindPoint::eCompute);

            context.renderer.bindStorageImage(*pipeline, context, *destinationTexture, 0, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

            // transition destination resource to a writeable format
            destinationTexture->getImage().transitionLayoutInline(cmds, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

            // copy/transform
            std::size_t groupCountX = (destinationTexture->getSize().width+31) / 32;
            std::size_t groupCountY = (destinationTexture->getSize().height+31) / 32;
            cmds.dispatch(groupCountX, groupCountY, 1);

            // transition destination resource into sampling format
            destinationTexture->getImage().transitionLayoutInline(cmds, vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal);

        } else {
            auto pipeline = context.renderer.getOrCreatePipelineFullPath("resources/pipelines/compute/debug-texture-viewer.pipeline", reinterpret_cast<std::uint64_t>(destinationTexture.get()));

            pipeline->bind({}, context, cmds, vk::PipelineBindPoint::eCompute);


            auto& inputImage = getTexture(sourceResource, context.swapchainIndex);

            context.renderer.bindTexture(*pipeline, context, inputImage, 0, 0, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
            context.renderer.bindSampler(*pipeline, context, driver.getLinearSampler(), 0, 1);
            context.renderer.bindSampler(*pipeline, context, driver.getNearestSampler(), 0, 2);
            context.renderer.bindStorageImage(*pipeline, context, *destinationTexture, 0, 3, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

            // transition destination resource to a writeable format
            destinationTexture->getImage().transitionLayoutInline(cmds, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

            // transition source resource to a sampling format
            if(sourceResource.layout != vk::ImageLayout::eGeneral)
                inputImage.getImage().transitionLayoutInline(cmds, sourceResource.layout, vk::ImageLayout::eGeneral); // TODO: aspect?

            // copy/transform
            std::size_t groupCountX = (destinationTexture->getSize().width+31) / 32;
            std::size_t groupCountY = (destinationTexture->getSize().height+31) / 32;
            cmds.dispatch(groupCountX, groupCountY, 1);

            // transition source resource back to its expected format
            if(sourceResource.layout != vk::ImageLayout::eGeneral)
                inputImage.getImage().transitionLayoutInline(cmds, vk::ImageLayout::eGeneral, sourceResource.layout); // TODO: aspect?

            // transition destination resource into sampling format
            destinationTexture->getImage().transitionLayoutInline(cmds, vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }

    void Graph::execute(const Render::Context& data, vk::CommandBuffer& cmds) {
        ZoneScoped;

        clickedResourceForRender = clickedResourceForMain;
        hoveredResourceForRender = hoveredResourceForMain;

        std::string id = "";
        for(auto* pass : sortedPasses) {
#ifdef TRACY_ENABLE
            std::string passName = "Execute pass ";
            GetVulkanDriver().setFormattedMarker(cmds, "Pass %s", pass->getName().data());

            if(hoveredResourceForRender || clickedResourceForRender) {
                auto isAnInput = [&](const FrameResource& resource) {
                    for(const auto& o : pass->getInputs()) {
                        if(o.id == resource.id) {
                            return true;
                        }
                    }

                    for(const auto& o : pass->getInputOutputs()) {
                        if(o.id == resource.id) {
                            return true;
                        }
                    }
                    return false;
                };

                if(hoveredResourceForRender && isAnInput(*hoveredResourceForRender)) {
                    drawViewer(data, *hoveredResourceForRender, hoveredResourceViewer, cmds);
                }
                if(clickedResourceForRender && isAnInput(*clickedResourceForRender)) {
                    drawViewer(data, *clickedResourceForRender, clickedResourceViewer, cmds);
                }
            }

            passName += pass->getName();
            id += pass->getName();
            id += " ";
            ZoneScopedN("Execute pass");
#endif
            pass->execute(data, cmds);

            if(hoveredResourceForRender || clickedResourceForRender) {
                auto isAnOutput = [&](const FrameResource& resource) {
                    for(const auto& o : pass->getOutputs()) {
                        if(o.id == resource.id) {
                            return true;
                        }
                    }

                    for(const auto& o : pass->getInputOutputs()) {
                        if(o.id == resource.id) {
                            return true;
                        }
                    }
                    return false;
                };

                if(hoveredResourceForRender && isAnOutput(*hoveredResourceForRender)) {
                    drawViewer(data, *hoveredResourceForRender, hoveredResourceViewer, cmds);
                }
                if(clickedResourceForRender && isAnOutput(*clickedResourceForRender)) {
                    drawViewer(data, *clickedResourceForRender, clickedResourceViewer, cmds);
                }
            }
        }
    }

    Texture& Graph::getTexture(const FrameResource& resource, size_t frameIndex) const {
        return driver.getResourceRepository().getTexture(resource, frameIndex);
    }

    Texture& Graph::getTexture(const Carrot::UUID& resourceID, size_t frameIndex) const {
        return driver.getResourceRepository().getTexture(resourceID, frameIndex);
    }

    static vk::ImageUsageFlags computeUsages(const FrameResource& resource) {
        vk::ImageUsageFlags wanted = GetVulkanDriver().getResourceRepository().getTextureUsages(resource.rootID);
        switch (resource.type) {
            case ResourceType::RenderTarget:
                wanted |= ForcedRenderTargetFlags;
            break;
            case ResourceType::StorageImage:
                if (resource.format != vk::Format::eR64Uint) {
                    wanted |= ForcedStorageRGBImageFlags;
                } else {
                    wanted |= ForcedStorageImageFlags;
                }
            break;

            default: verify(false, "unreachable");
        }
        return wanted;
    }

    Render::Texture& Graph::createTexture(const FrameResource& resource, size_t frameIndex, const vk::Extent2D& viewportSize) {
        return driver.getResourceRepository().createTexture(resource, frameIndex,
            computeUsages(resource),
            viewportSize);
    }

    Render::Texture& Graph::getOrCreateTexture(const FrameResource& resource, size_t frameIndex, const vk::Extent2D& viewportSize) {
        return driver.getResourceRepository().getOrCreateTexture(resource, frameIndex,
            computeUsages(resource),
            viewportSize);
    }

    BufferAllocation& Graph::getBuffer(const FrameResource& resource, size_t frameIndex) const {
        return driver.getResourceRepository().getBuffer(resource, frameIndex);
    }

    BufferAllocation& Graph::getBuffer(const Carrot::UUID& resourceID, size_t frameIndex) const {
        return driver.getResourceRepository().getBuffer(resourceID, frameIndex);
    }

    BufferAllocation& Graph::getOrCreateBuffer(const FrameResource& resource, size_t frameIndex) {
        return driver.getResourceRepository().getOrCreateBuffer(resource, frameIndex,
            driver.getResourceRepository().getBufferUsages(resource.rootID) | ForcedBufferFlags);
    }

    void Graph::onSwapchainImageCountChange(size_t newCount) {
        for(auto& [n, pass] : passes) {
            pass->onSwapchainImageCountChange(newCount);
        }
    }

    void Graph::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) {
        for(auto& [n, pass] : passes) {
            pass->onSwapchainSizeChange(window, newWidth, newHeight);
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
