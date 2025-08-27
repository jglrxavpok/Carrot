//
// Created by jglrxavpok on 02/11/2023.
//

#include "ImGuiBackend.h"

#include <IconsFontAwesome6.h>
#include <core/Macros.h>
#include <engine/render/resources/Texture.h>
#include <engine/Engine.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/render/resources/ResourceAllocator.h>
#include <core/render/VertexTypes.h>

namespace Carrot::Render {
    constexpr const char* PipelinePath = "resources/pipelines/imgui.pipeline";
    constexpr int MaxTextures = 1024;

    struct PImpl {
        std::unique_ptr<Texture> fontsTexture;

        struct PerFrameData {
            std::shared_ptr<Pipeline> pipeline;
            Render::PerFrame<Carrot::BufferAllocation> vertexBuffers; // vertex buffer per frame
            Render::PerFrame<Carrot::BufferAllocation> indexBuffers; // index buffer per frame
            Render::PerFrame<Carrot::BufferAllocation> stagingBuffers;
            Render::PerFrame<vk::UniqueSemaphore> bufferCopySync;
            Render::PerFrame<bool> rebindTextures; // rebind entire texture array (with a white texture)

            std::vector<Carrot::ImGuiVertex> vertexStorage; // temporary storage to store vertices before copy to GPU-visible memory
            std::vector<std::uint32_t> indexStorage; // temporary storage to store indices before copy to GPU-visible memory

            std::unordered_map<ImTextureID, std::size_t> textureIndices;
            Viewport* pViewport = nullptr;
        };

        std::unordered_map<WindowID, PerFrameData> perWindow;

        void initPerFrameData(VulkanRenderer& renderer, WindowID w, Viewport* pViewport) {
            PerFrameData& v = perWindow[w];
            v.pipeline = renderer.getOrCreatePipelineFullPath(PipelinePath, (std::uint64_t)w); // one per window to avoid texture binding conflicts
            v.pViewport = pViewport;

            std::size_t imageCount = MAX_FRAMES_IN_FLIGHT;
            v.vertexBuffers.resize(imageCount);
            v.indexBuffers.resize(imageCount);
            v.stagingBuffers.resize(imageCount);
            v.bufferCopySync.resize(imageCount);
            v.rebindTextures.resize(imageCount);

            vk::SemaphoreCreateInfo semaphoreInfo{};
            for(std::size_t i = 0; i < imageCount; i++) {
                v.bufferCopySync[i] = renderer.getLogicalDevice().createSemaphoreUnique(semaphoreInfo, renderer.getVulkanDriver().getAllocationCallbacks());
                DebugNameable::nameSingle("ImGui buffer copy sync", *v.bufferCopySync[i]);

                v.rebindTextures[i] = true;
            }
        }
    };

    struct ImGuiRendererData {
        Window* pWindow = nullptr;
        Viewport* pViewport = nullptr;
    };

    ImGuiBackend::ImGuiBackend(VulkanRenderer& renderer): renderer(renderer) {
    }

    ImGuiBackend::~ImGuiBackend() {
        cleanup();
    }

    static void createWindowImGuiCallback(ImGuiViewport* pViewport) {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiBackend* pThis = (ImGuiBackend*)io.BackendRendererUserData;
        pThis->createWindowImGui(pViewport);
    }

    static void setWindowSizeImGuiCallback(ImGuiViewport* vp, ImVec2 size) {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiBackend* pThis = (ImGuiBackend*)io.BackendRendererUserData;
        ImGuiRendererData* pRendererData = static_cast<ImGuiRendererData*>(vp->RendererUserData);
        if(pRendererData == nullptr) { // not external window
            return;
        }

        GetRenderer().waitForRenderToComplete();
        WaitDeviceIdle();
        pRendererData->pWindow->setWindowSize(size.x, size.y);
        GetEngine().recreateSwapchain(*pRendererData->pWindow);
    }

    static void renderWindowImGuiCallback(ImGuiViewport* vp, void* render_arg) {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiBackend* pThis = (ImGuiBackend*)io.BackendRendererUserData;
        ImGuiRendererData* pRendererData = static_cast<ImGuiRendererData*>(vp->RendererUserData);
        if(pRendererData == nullptr) { // not external window
            return;
        }
        pThis->renderExternalWindowImGui(vp->DrawData, *pRendererData, GetRenderer().getFrameCount() % MAX_FRAMES_IN_FLIGHT, *((std::size_t*)render_arg));
    }

    static void destroyWindowImGui(ImGuiViewport* vp) {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiBackend* pThis = (ImGuiBackend*)io.BackendRendererUserData;
        ImGuiRendererData* pRendererData = static_cast<ImGuiRendererData*>(vp->RendererUserData);
        if(pRendererData == nullptr) { // not external window
            return;
        }
        GetEngine().destroyViewport(*pRendererData->pViewport);
        vp->RendererUserData = nullptr;
        GetEngine().destroyWindow(*pRendererData->pWindow);
    }

    void ImGuiBackend::initResources() {
        pImpl = new PImpl;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigWindowsResizeFromEdges = true;
        io.ConfigFlags |= GetConfiguration().runInVR ? 0 : ImGuiConfigFlags_ViewportsEnable;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsClassic();

        {
            auto& style = ImGui::GetStyle();
            style.FrameRounding = 2.0f;
            style.FrameBorderSize = 1.0f;
            style.WindowBorderSize = 1.0f;

            auto& colors = style.Colors;
        }

        // Setup Platform/Renderer backends
        // TODO: move to dedicated style file
        float baseFontSize = 14.0f; // 13.0f is the size of the default font. Change to the font size you use.
        auto font = io.Fonts->AddFontFromFileTTF(Carrot::toString(GetVFS().resolve(IO::VFS::Path("resources/fonts/Roboto-Medium.ttf")).u8string()).c_str(), baseFontSize);

        // from https://github.com/juliettef/IconFontCppHeaders/tree/main/README.md
        float iconFontSize = baseFontSize;// * 2.0f / 3.0f; // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly

        // merge in icons from Font Awesome
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = iconFontSize;
        io.Fonts->AddFontFromFileTTF( Carrot::toString(GetVFS().resolve(IO::VFS::Path("resources/fonts/fa-solid-900.ttf")).u8string()).c_str(),
                                      iconFontSize, &icons_config, icons_ranges );

        icons_config.MergeMode = false;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = iconFontSize*4;

        bigIconsFont = io.Fonts->AddFontFromFileTTF( Carrot::toString(GetVFS().resolve(IO::VFS::Path("resources/fonts/fa-solid-900.ttf")).u8string()).c_str(),
                                      iconFontSize*4, &icons_config, icons_ranges );

        io.Fonts->Build();

        io.BackendRendererName = "Carrot ImGui Backend";
        io.BackendRendererUserData = this;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

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

        pImpl->initPerFrameData(renderer, renderer.getEngine().getMainWindow().getWindowID(), nullptr /* directly rendered on top of main viewport */);

        // resize buffers for frame-by-frame storage
        resizeStorage(MAX_FRAMES_IN_FLIGHT);

        ImGui_ImplGlfw_InitForVulkan(renderer.getEngine().getMainWindow().getGLFWPointer(), true);

        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            IM_ASSERT(platform_io.Platform_CreateVkSurface != NULL && "Platform needs to setup the CreateVkSurface handler.");
        platform_io.Renderer_CreateWindow = createWindowImGuiCallback;
        platform_io.Renderer_SetWindowSize = setWindowSizeImGuiCallback;
        platform_io.Renderer_RenderWindow = renderWindowImGuiCallback;
        platform_io.Renderer_DestroyWindow = destroyWindowImGui;
        platform_io.Renderer_SwapBuffers = nullptr; // swapping is handled by engine with Window and the main loop
    }

    void ImGuiBackend::newFrame() {
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

    void ImGuiBackend::render(const Carrot::Render::Context& renderContext, WindowID windowID, ImDrawData* pDrawData) {
        static_assert(sizeof(ImDrawVert) == sizeof(Carrot::ImGuiVertex));

        std::vector<Carrot::ImGuiVertex>& vertices = pImpl->perWindow[windowID].vertexStorage;
        std::vector<std::uint32_t>& indices = pImpl->perWindow[windowID].indexStorage;

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

        pImpl->perWindow[windowID].vertexBuffers[renderContext.frameIndex] = GetResourceAllocator().allocateDeviceBuffer(vertices.size()*sizeof(Carrot::ImGuiVertex), vk::BufferUsageFlagBits::eVertexBuffer);
        pImpl->perWindow[windowID].indexBuffers[renderContext.frameIndex] = GetResourceAllocator().allocateDeviceBuffer(indices.size()*sizeof(std::uint32_t), vk::BufferUsageFlagBits::eIndexBuffer);
        Carrot::BufferView& vertexBuffer = pImpl->perWindow[windowID].vertexBuffers[renderContext.frameIndex].view;
        Carrot::BufferView& indexBuffer = pImpl->perWindow[windowID].indexBuffers[renderContext.frameIndex].view;

        std::size_t totalStagingSize = vertexBuffer.getSize() + indexBuffer.getSize();
        auto& stagingBufferAlloc = pImpl->perWindow[windowID].stagingBuffers[renderContext.frameIndex];
        stagingBufferAlloc = GetResourceAllocator().allocateStagingBuffer(totalStagingSize);
        auto& stagingBuffer = stagingBufferAlloc.view;

        // upload data to staging buffer
        stagingBuffer.directUpload(vertices.data(), vertexBuffer.getSize());
        stagingBuffer.directUpload(indices.data(), indexBuffer.getSize(), vertexBuffer.getSize()/*offset*/);
        auto& copySyncSemaphore = pImpl->perWindow[windowID].bufferCopySync[renderContext.frameIndex];
        renderer.getVulkanDriver().performSingleTimeTransferCommands([&](vk::CommandBuffer& cmds) {
            stagingBuffer.subView(0, vertexBuffer.getSize()).cmdCopyTo(cmds, vertexBuffer);
            stagingBuffer.subView(vertexBuffer.getSize(), indexBuffer.getSize()).cmdCopyTo(cmds, indexBuffer);
        }, false/* don't wait for this copy */, {}, static_cast<vk::PipelineStageFlags2>(0), *copySyncSemaphore);
        renderer.getEngine().addWaitSemaphoreBeforeRendering(renderContext, vk::PipelineStageFlagBits::eVertexInput, *copySyncSemaphore);

        Render::Packet& packet = renderer.makeRenderPacket(Render::PassEnum::ImGui, Render::PacketType::DrawIndexedInstanced, renderContext);
        packet.pipeline = pImpl->perWindow[windowID].pipeline;
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

        auto& textureIndexMap = pImpl->perWindow[windowID].textureIndices;
        textureIndexMap.clear();

        if(pImpl->perWindow[windowID].rebindTextures[renderContext.frameIndex]) {
            for (int i = 0; i < MaxTextures; ++i) {
                renderer.bindTexture(*pImpl->perWindow[windowID].pipeline, renderContext, *pImpl->fontsTexture, 0, 0,
                                     vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D,
                                     i);
            }
            pImpl->perWindow[windowID].rebindTextures[renderContext.frameIndex] = false;
        }

        int drawIndex = 0;
        auto& drawCommand = packet.commands.emplace_back().drawIndexedInstanced;
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
                        renderer.bindTexture(*pImpl->perWindow[windowID].pipeline, renderContext, texture, 0, 0,
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
                            .width = static_cast<std::uint32_t>(std::min(viewportWidth, pcmd->ClipRect.z - pcmd->ClipRect.x)),
                            .height = static_cast<std::uint32_t>(std::min(viewportHeight, pcmd->ClipRect.w - pcmd->ClipRect.y)),
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

    void ImGuiBackend::record(vk::CommandBuffer& cmds, const Render::CompiledPass& pass, const Carrot::Render::Context& renderContext) {
        renderer.recordPassPackets(PassEnum::ImGui, pass, renderContext, cmds);
    }

    void ImGuiBackend::resizeStorage(std::size_t newCount) {
        for(auto& [_, v] : pImpl->perWindow) {
            v.vertexBuffers.resize(newCount);
            v.indexBuffers.resize(newCount);
            v.stagingBuffers.resize(newCount);
            v.bufferCopySync.resize(newCount);
            v.rebindTextures.resize(newCount);

            vk::SemaphoreCreateInfo semaphoreInfo{};
            for(std::size_t i = 0; i < newCount; i++) {
                v.bufferCopySync[i] = renderer.getLogicalDevice().createSemaphoreUnique(semaphoreInfo, renderer.getVulkanDriver().getAllocationCallbacks());
                DebugNameable::nameSingle("ImGui buffer copy sync", *v.bufferCopySync[i]);

                v.rebindTextures[i] = true;
            }
        }
    }

    ImFont* ImGuiBackend::getBigIconsFont() {
        return bigIconsFont;
    }

    void ImGuiBackend::createWindowImGui(ImGuiViewport* pViewport) {
        ImU64 createdSurface;
        VkResult err = (VkResult)ImGui::GetPlatformIO().Platform_CreateVkSurface(pViewport,
                                                                                 (ImU64)(VkInstance)renderer.getVulkanDriver().getInstance(),
                                                                                 renderer.getVulkanDriver().getAllocationCallbacks(),
                                                                                 &createdSurface);
        vk::SurfaceKHR surface = (vk::SurfaceKHR)(VkSurfaceKHR)createdSurface;
        Window& externalWindow = renderer.getEngine().createAdoptedWindow(surface);
        ImGuiRendererData* pRendererUserData = new ImGuiRendererData;
        pViewport->RendererUserData = pRendererUserData;
        externalWindow.createSwapChain();

        pRendererUserData->pWindow = &externalWindow;

        Render::Viewport& viewport = renderer.getEngine().createViewport(externalWindow);
        Render::GraphBuilder renderGraphBuilder { renderer.getVulkanDriver(), externalWindow };
        struct ImGuiRenderPassData {
            Render::FrameResource targetTexture;
        };
        renderGraphBuilder.addPass<ImGuiRenderPassData>("ImGui external window",
                                                        [this](GraphBuilder& graph, Pass<ImGuiRenderPassData>& pass, ImGuiRenderPassData& data) {
                                                            vk::ClearColorValue clearColor { 0.0, 0.0, 0.0, 1.0f };
                                                            data.targetTexture = graph.write(graph.getSwapchainImage(), vk::AttachmentLoadOp::eClear, vk::ImageLayout::eColorAttachmentOptimal, clearColor);
                                                            graph.present(data.targetTexture);
        },
                                                        [this](const CompiledPass& pass, const Render::Context& renderContext, ImGuiRenderPassData& data, vk::CommandBuffer& cmds) {
                                                            record(cmds, pass, renderContext);
        });
        pRendererUserData->pViewport = &viewport;
        viewport.setRenderGraph(std::move(renderGraphBuilder.compile()));
        pImpl->initPerFrameData(renderer, externalWindow.getWindowID(), &viewport);
    }

    void ImGuiBackend::renderExternalWindowImGui(ImDrawData* pDrawData, ImGuiRendererData& rendererData, u8 frameIndex, std::size_t swapchainIndex) {
        Carrot::Render::Context renderContext = renderer.getEngine().newRenderContext(frameIndex, swapchainIndex, *rendererData.pViewport);
        render(renderContext, rendererData.pWindow->getWindowID(), pDrawData);
    }
} // Carrot::Render