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

namespace ed = ax::NodeEditor;

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
        ed::Config config;
        nodesContext = ed::CreateEditor(&config);
    }

    static std::uint32_t uniqueID = 1;

    static std::uint32_t getNodeID(std::uint32_t passIndex) {
        static std::unordered_map<std::uint32_t, std::uint32_t> nodes;

        if(nodes.find(passIndex) == nodes.end()) {
            nodes[passIndex] = uniqueID++;
        }
        return nodes[passIndex];
    }
}

struct PinKey {
    std::uint32_t passIndex;
    std::uint32_t pinID;

    auto operator<=>(const PinKey&) const = default;
};

template<>
struct std::hash<PinKey> {
    std::size_t operator()(const PinKey& k) const {
        return (((std::uint64_t)k.passIndex) << 32) | k.pinID;
    }
};

namespace Carrot::Render {
    static std::uint32_t getInputPinID(const Carrot::UUID& uuid) {
        static std::unordered_map<Carrot::UUID, std::uint32_t> pinIDs;

        if(pinIDs.find(uuid) == pinIDs.end()) {
            pinIDs[uuid] = uniqueID++;
        }
        return pinIDs[uuid];
    }

    static std::uint32_t getOutputPinID(const Carrot::UUID& uuid) {
        static std::unordered_map<Carrot::UUID, std::uint32_t> pinIDs;

        if(pinIDs.find(uuid) == pinIDs.end()) {
            pinIDs[uuid] = uniqueID++;
        }
        return pinIDs[uuid];
    }

    static std::uint32_t getLinkID(const Carrot::UUID& in, const Carrot::UUID& out) {
        static std::unordered_map<std::uint32_t, std::unordered_map<std::uint32_t, std::uint32_t>> linkIDs;

        auto& inLinks = linkIDs[getInputPinID(in)];
        const auto outID = getOutputPinID(out);
        if(inLinks.find(outID) == inLinks.end()) {
            inLinks[outID] = uniqueID++;
        }
        return inLinks[outID];
    }

    void Graph::drawPassNodes(const Render::Context& context, Render::CompiledPass* pass, std::uint32_t passIndex) {
        std::uint32_t nodeID = getNodeID(passIndex);
        ed::BeginNode(passIndex+1);

        ImGui::PushID(passIndex);

        ImGui::BeginVertical("node");
        ImGui::Spring();

        ImGui::Text("%s", std::string(pass->getName()).c_str());

        ImGui::Spring();

        ImGui::BeginHorizontal("content");

        ImGui::BeginVertical("inputs");
        ed::PushStyleVar(ed::StyleVar_PivotAlignment, ImVec2(0, 0.5f));
        ed::PushStyleVar(ed::StyleVar_PivotSize, ImVec2(0, 0));

        std::uint32_t inputPinIndex = 0;
        for(const auto& i : pass->getInputs()) {
            ed::BeginPin(getInputPinID(i.id), ed::PinKind::Input);
            ImGui::Text("> %u", inputPinIndex++);
            ed::EndPin();

            if(ImGui::IsItemHovered()) {
                hoveredResource = &i;
            }
        }
        /*
        for (auto& pin : inputs) {
            ed::BeginPin(graph.getEditorID(pin->id), ed::PinKind::Input);
            ImGui::BeginHorizontal(&pin->id);
            ImGui::Text("> %s", pin->name.c_str());
            auto icon = graph.getImGuiTextures().getExpressionType(pin->getExpressionType());
            ImGui::Image(icon, ImVec2(10,10));

            ImGui::EndHorizontal();
            ed::EndPin();
        }
         */
        ImGui::EndVertical();

        ImGui::BeginVertical("center");
        ImGui::Text("%s", pass->getName().data());
        //renderCenter();
        ImGui::EndVertical();


        /*if(inputs.empty()) */{
            ImGui::Spring(1);
        }
        ImGui::BeginVertical("outputs");
        ed::PushStyleVar(ed::StyleVar_PivotAlignment, ImVec2(1.0f, 0.5f));
        ed::PushStyleVar(ed::StyleVar_PivotSize, ImVec2(0, 0));

       /* for (auto& pin : outputs) {
            ed::BeginPin(graph.getEditorID(pin->id), ed::PinKind::Output);
            ImGui::BeginHorizontal(&pin->id);

            auto icon = graph.getImGuiTextures().getExpressionType(pin->getExpressionType());
            ImGui::Image(icon, ImVec2(10,10));
            ImGui::Text("%s >", pin->name.c_str());

            ImGui::EndHorizontal();
            ed::EndPin();
        }*/
        std::uint32_t outputPinIndex = 0;
        for(const auto& o : pass->getOutputs()) {
            ed::BeginPin(getOutputPinID(o.id), ed::PinKind::Output);
            ImGui::Text("%u >", outputPinIndex++);
            ed::EndPin();

            if(ImGui::IsItemHovered()) {
                hoveredResource = &o;
            }
        }
        ImGui::EndVertical();

        /*if(outputs.empty()) */{
            ImGui::Spring(1);
        }

        ImGui::EndHorizontal();
        ImGui::EndVertical();

        ImGui::PopID();
        ed::EndNode();

        for(const auto& i : pass->getInputs()) {
            ed::Link(getLinkID(i.id, i.parentID), getInputPinID(i.id), getOutputPinID(i.parentID));
        }
    }

    void Graph::onFrame(const Render::Context& context) {
        const bool debug = true;

        static Graph* graphToDebug = nullptr;
        if(debug) {
            if(ImGui::Begin("Debug render graphs")) {
                ImGui::Separator();

                std::string id = Carrot::sprintf("%x , %llu passes", (std::uint64_t)this, sortedPasses.size());
                if(ImGui::RadioButton(id.c_str(), graphToDebug == this)) {
                    graphToDebug = this;
                }

                if(graphToDebug == this) {
                    ed::SetCurrentEditor((ed::EditorContext*)nodesContext);
                    ed::EnableShortcuts(true);

                    hoveredResource = nullptr;
                    ed::Begin("Render graph debug");
                    std::uint32_t index = 0;
                    for (auto *pass: sortedPasses) {
                        drawPassNodes(context, pass, index++);
                    }
                    ed::End();
                    ed::EnableShortcuts(false);

                    if(hoveredResource != nullptr) {
                        ImGui::BeginTooltip();
                        auto& texture = GetVulkanDriver().getTextureRepository().get(*hoveredResource, context.swapchainIndex);
                        float aspectRatio = texture.getSize().width / (float) texture.getSize().height;
                        ImGui::Text("%s (%u x %u x %u)", hoveredResource->rootID.toString().c_str(), texture.getSize().width, texture.getSize().height, texture.getSize().depth);
                        ImGui::Image(texture.getImguiID(hoveredResource->format), ImVec2(aspectRatio * 512.0f, 512.0f));
                        ImGui::EndTooltip();
                    }
                }
            }
            ImGui::End();
        }
    }

    void Graph::execute(const Render::Context& data, vk::CommandBuffer& cmds) {
        ZoneScoped;

        std::string id = "";
        for(auto* pass : sortedPasses) {
#ifdef TRACY_ENABLE
            std::string passName = "Execute pass ";
            GetVulkanDriver().setFormattedMarker(cmds, "Pass %s", pass->getName().data());

            passName += pass->getName();
            id += pass->getName();
            id += " ";
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
