//
// Created by jglrxavpok on 02/11/2023.
//

#include "ImGuiBackend.h"
#include <core/Macros.h>
#include <engine/render/resources/Texture.h>
#include <engine/Engine.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/render/resources/ResourceAllocator.h>
#include <core/render/VertexTypes.h>

namespace Carrot::Render {
    // TODO: multi-window support
    constexpr const char* PipelinePath = "resources/pipelines/imgui.json";

    struct PImpl {
        std::unique_ptr<Texture> fontsTexture;
        std::shared_ptr<Pipeline> pipeline;
        Render::PerFrame<Carrot::BufferAllocation> vertexBuffers; // vertex buffer per frame
        Render::PerFrame<Carrot::BufferAllocation> indexBuffers; // index buffer per frame
        Render::PerFrame<Carrot::BufferAllocation> stagingBuffers;
        Render::PerFrame<vk::UniqueSemaphore> bufferCopySync;

        std::vector<Carrot::ImGuiVertex> vertexStorage; // temporary storage to store vertices before copy to GPU-visible memory
        std::vector<std::uint32_t> indexStorage; // temporary storage to store indices before copy to GPU-visible memory

        std::unordered_map<ImTextureID, std::size_t> textureIndices;
    };

    ImGuiBackend::ImGuiBackend(VulkanRenderer& renderer): renderer(renderer) {
    }

    ImGuiBackend::~ImGuiBackend() {
        cleanup();
    }

    void ImGuiBackend::initResources() {
        pImpl = new PImpl;
        ImGuiIO& io = ImGui::GetIO();
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        std::unique_ptr<Image> fontsImage = std::make_unique<Image>(GetVulkanDriver(),
                                                                    vk::Extent3D { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                                                                    vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                                                                    vk::Format::eR8G8B8A8Unorm);
        fontsImage->stageUpload(std::span{ pixels, static_cast<std::size_t>(width * height * 4) });
        pImpl->fontsTexture = std::make_unique<Texture>(std::move(fontsImage));
        io.Fonts->SetTexID(pImpl->fontsTexture->getImguiID());
        pImpl->pipeline = renderer.getOrCreatePipelineFullPath(PipelinePath);

        // TODO: install renderer_renderwindow imgui hook

        // resize buffers for frame-by-frame storage
        onSwapchainImageCountChange(renderer.getSwapchainImageCount());

        ImGui_ImplGlfw_InitForVulkan(renderer.getVulkanDriver().getWindow().getGLFWPointer(), true);
    }

    void ImGuiBackend::newFrame() {
        // TODO: update mouse pos
        // TODO: update mouse buttons
        // TODO: update keys
        // TODO: update gamepad?
        //TODO;
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiBackend::cleanup() {
        if(!pImpl) {
            return;
        }
        delete pImpl;
        pImpl = nullptr;
    }

    void ImGuiBackend::render(const Carrot::Render::Context& renderContext, ImDrawData* pDrawData) {
        static_assert(sizeof(ImDrawVert) == sizeof(Carrot::ImGuiVertex));

        std::vector<Carrot::ImGuiVertex>& vertices = pImpl->vertexStorage;
        std::vector<std::uint32_t>& indices = pImpl->indexStorage;

        // start offsets per command list
        std::vector<std::size_t> vertexStarts;
        std::vector<std::size_t> indexStarts;

        vertices.clear();
        indices.clear();

        // merge all vertices and indices into two big buffers
        for (int n = 0; n < pDrawData->CmdListsCount; n++) {
            const ImDrawList* pCmdList = pDrawData->CmdLists[n];
            std::size_t vertexCount = pCmdList->VtxBuffer.size();
            std::size_t indexCount = pCmdList->IdxBuffer.size();

            std::size_t vertexStart = vertices.size();
            std::size_t indexStart = indices.size();
            vertexStarts.push_back(vertexStart);
            indexStarts.push_back(indexStart);

            vertices.resize(vertexStart + vertexCount);
            indices.resize(indexStart + indexCount);
            // ImGui has 16-bit indices, but Carrot currently only supports 32-bit
            for(std::size_t i = 0; i < indexCount; i++) {
                indices[indexStart + i] = pCmdList->IdxBuffer[i];
            }

            memcpy(vertices.data() + vertexStart, pCmdList->VtxBuffer.Data, vertexCount * sizeof(Carrot::ImGuiVertex));
        }

        if(vertices.size() == 0) {
            return;// nothing to render
        }

        pImpl->vertexBuffers[renderContext.swapchainIndex] = GetResourceAllocator().allocateDeviceBuffer(vertices.size()*sizeof(Carrot::ImGuiVertex), vk::BufferUsageFlagBits::eVertexBuffer);
        pImpl->indexBuffers[renderContext.swapchainIndex] = GetResourceAllocator().allocateDeviceBuffer(indices.size()*sizeof(std::uint32_t), vk::BufferUsageFlagBits::eIndexBuffer);
        Carrot::BufferView& vertexBuffer = pImpl->vertexBuffers[renderContext.swapchainIndex].view;
        Carrot::BufferView& indexBuffer = pImpl->indexBuffers[renderContext.swapchainIndex].view;

        std::size_t totalStagingSize = vertexBuffer.getSize() + indexBuffer.getSize();
        auto& stagingBufferAlloc = pImpl->stagingBuffers[renderContext.swapchainIndex];
        stagingBufferAlloc = GetResourceAllocator().allocateStagingBuffer(totalStagingSize);
        auto& stagingBuffer = stagingBufferAlloc.view;

        // upload data to staging buffer
        stagingBuffer.directUpload(vertices.data(), vertexBuffer.getSize());
        stagingBuffer.directUpload(indices.data(), indexBuffer.getSize(), vertexBuffer.getSize()/*offset*/);
        auto& copySyncSemaphore = pImpl->bufferCopySync[renderContext.swapchainIndex];
        renderer.getVulkanDriver().performSingleTimeTransferCommands([&](vk::CommandBuffer& cmds) {
            stagingBuffer.subView(0, vertexBuffer.getSize()).cmdCopyTo(cmds, vertexBuffer);
            stagingBuffer.subView(vertexBuffer.getSize(), indexBuffer.getSize()).cmdCopyTo(cmds, indexBuffer);
        }, false/* don't wait for this copy */, {}, static_cast<vk::PipelineStageFlags>(0), *copySyncSemaphore);
        renderer.getEngine().addWaitSemaphoreBeforeRendering(vk::PipelineStageFlagBits::eVertexInput, *copySyncSemaphore);

        Render::Packet& packet = renderer.makeRenderPacket(Render::PassEnum::ImGui, renderContext);
        packet.pipeline = pImpl->pipeline;
        packet.vertexBuffer = vertexBuffer;
        packet.indexBuffer = indexBuffer;

        struct {
            glm::vec2 translation;
            glm::vec2 scale;
            std::uint32_t textureIndex;
        } displayConstantData;
        // map from 0,0 - w,h to -1,-1 - 1,1
        displayConstantData.scale.x = 2.0f / pDrawData->DisplaySize.x;
        displayConstantData.scale.y = 2.0f / pDrawData->DisplaySize.y;
        displayConstantData.translation.x = -pDrawData->DisplayPos.x * displayConstantData.scale.x - 1.0f;
        displayConstantData.translation.y = -pDrawData->DisplayPos.y * displayConstantData.scale.y - 1.0f;

        Packet::PushConstant& displayConstant = packet.addPushConstant("Display", vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
        displayConstant.setData(displayConstantData);

        float viewportWidth = pDrawData->DisplaySize.x * pDrawData->FramebufferScale.x;
        float viewportHeight = pDrawData->DisplaySize.y * pDrawData->FramebufferScale.y;
        if (viewportWidth <= 0 || viewportHeight <= 0)
            return;

        packet.viewportExtents = vk::Viewport {
            .x = 0,
            .y = 0,
            .width = viewportWidth,
            .height = viewportHeight,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        auto& textureIndexMap = pImpl->textureIndices;
        textureIndexMap.clear();

        int drawIndex = 0;
        auto& drawCommand = packet.drawCommands.emplace_back();
        for (int n = 0; n < pDrawData->CmdListsCount; n++) {
            const ImDrawList* cmd_list = pDrawData->CmdLists[n];
            std::size_t commandListVertexOffset = vertexStarts[n];
            std::size_t commandListIndexOffset = indexStarts[n];
            for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
                if (pcmd->UserCallback) {
                    pcmd->UserCallback(cmd_list, pcmd);
                } else {
                    // The texture for the draw call is specified by pcmd->GetTexID().
                    // The vast majority of draw calls will use the Dear ImGui texture atlas, which value you have set yourself during initialization.
                    auto [iter, bInserted] = textureIndexMap.try_emplace(pcmd->GetTexID(), 0);

                    if(bInserted) {
                        iter->second = textureIndexMap.size() - 1;
                        Carrot::Render::Texture& texture = *((Carrot::Render::Texture*)pcmd->GetTexID());
                        renderer.bindTexture(*pImpl->pipeline, renderContext, texture, 0, 0,
                                             vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D,
                                             iter->second);
                    }
                    displayConstantData.textureIndex = iter->second;

                    // We are using scissoring to clip some objects. All low-level graphics API should support it.
                    // - If your engine doesn't support scissoring yet, you may ignore this at first. You will get some small glitches
                    //   (some elements visible outside their bounds) but you can fix that once everything else works!
                    // - Clipping coordinates are provided in imgui coordinates space:
                    //   - For a given viewport, draw_data->DisplayPos == viewport->Pos and draw_data->DisplaySize == viewport->Size
                    //   - In a single viewport application, draw_data->DisplayPos == (0,0) and draw_data->DisplaySize == io.DisplaySize, but always use GetMainViewport()->Pos/Size instead of hardcoding those values.
                    //   - In the interest of supporting multi-viewport applications (see 'docking' branch on github),
                    //     always subtract draw_data->DisplayPos from clipping bounds to convert them to your viewport space.
                    // - Note that pcmd->ClipRect contains Min+Max bounds. Some graphics API may use Min+Max, other may use Min+Size (size being Max-Min)
                    ImVec2 pos = pDrawData->DisplayPos;

                    vk::Rect2D scissorRect;
                    int scissorX = (int) (pcmd->ClipRect.x - pos.x);
                    int scissorY = (int) (pcmd->ClipRect.y - pos.y);
                    scissorRect.offset = vk::Offset2D {
                            .x = static_cast<std::int32_t>(std::max(0, scissorX)),
                            .y = static_cast<std::int32_t>(std::max(0, scissorY)),
                    };
                    scissorRect.extent = vk::Extent2D {
                            .width = static_cast<std::uint32_t>(std::min(viewportWidth, pcmd->ClipRect.z - pos.x)),
                            .height = static_cast<std::uint32_t>(std::min(viewportHeight, pcmd->ClipRect.w - pos.y)),
                    };
                    packet.scissor = scissorRect;

                    // force packets to be ordered. this is because the sort used before recording is not stable: similar packets are not guaranteed to stay in the same order
                    packet.transparentGBuffer.zOrder = drawIndex++;
                    // Render 'pcmd->ElemCount/3' indexed triangles.
                    // By default the indices ImDrawIdx are 16-bit, you can change them to 32-bit in imconfig.h if your engine doesn't support 16-bit indices.
                    drawCommand.instanceCount = 1;
                    drawCommand.indexCount = pcmd->ElemCount;
                    drawCommand.firstIndex = pcmd->IdxOffset + commandListIndexOffset;
                    drawCommand.vertexOffset = pcmd->VtxOffset + commandListVertexOffset;

                    displayConstant.setData(displayConstantData);
                    renderer.render(packet);
                }
            }
        }
    }

    void ImGuiBackend::record(vk::CommandBuffer& cmds, vk::RenderPass renderPass, const Carrot::Render::Context& renderContext) {
        renderer.recordPassPackets(PassEnum::ImGui, renderPass, renderContext, cmds);
    }

    void ImGuiBackend::onSwapchainImageCountChange(std::size_t newCount) {
        pImpl->vertexBuffers.resize(newCount);
        pImpl->indexBuffers.resize(newCount);
        pImpl->stagingBuffers.resize(newCount);
        pImpl->bufferCopySync.resize(newCount);

        vk::SemaphoreCreateInfo semaphoreInfo{};
        for(std::size_t i = 0; i < newCount; i++) {
            pImpl->bufferCopySync[i] = renderer.getLogicalDevice().createSemaphoreUnique(semaphoreInfo, renderer.getVulkanDriver().getAllocationCallbacks());
        }
    }
} // Carrot::Render