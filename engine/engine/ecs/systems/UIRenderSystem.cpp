//
// Created by jglrxavpok on 13/06/2026.
//

#include "UIRenderSystem.h"

#include <clay.h>
#include <engine/Engine.h>
#include <engine/render/resources/ResourceAllocator.h>

namespace Carrot::ECS {
    UIRenderSystem::UIRenderSystem(World& world)
                : RenderSystem<TransformComponent>(world) {
        rectanglePipelineResource = AsyncResource<Pipeline, false>(GetAssetServer().loadPipelineTask("resources/pipelines/ui/rectangle.pipeline"));
        imagePipelineResource = AsyncResource<Pipeline, false>(GetAssetServer().loadPipelineTask("resources/pipelines/ui/image.pipeline"));
        testImage = AsyncResource<Render::Texture, false>(GetAssetServer().loadTextureTask("resources/icon128.ktx2"));
    }

    // returns index of first index of geometry
    static u32 generateQuadGeometry(float x, float y, float w, float h, const glm::vec2& uv0, const glm::vec2& uv1, u32 color, Carrot::Vector<ImGuiVertex>& vertices, Carrot::Vector<u32>& indices) {
        u64 vertexOffset = vertices.size();
        vertices.resize(vertexOffset + 4);
        vertices[vertexOffset + 0].color = color;
        vertices[vertexOffset + 0].pos = {x, y};
        vertices[vertexOffset + 0].uv = {uv0.x, uv0.y};

        vertices[vertexOffset + 1].color = color;
        vertices[vertexOffset + 1].pos = {x+w, y};
        vertices[vertexOffset + 1].uv = {uv1.x, uv0.y};

        vertices[vertexOffset + 2].color = color;
        vertices[vertexOffset + 2].pos = {x+w, y+h};
        vertices[vertexOffset + 2].uv = {uv1.x, uv1.y};

        vertices[vertexOffset + 3].color = color;
        vertices[vertexOffset + 3].pos = {x, y+h};
        vertices[vertexOffset + 3].uv = {uv0.x, uv1.y};

        u32 indexOffset = indices.size();
        indices.resize(indexOffset + 6);

        indices[indexOffset+0] = vertexOffset+0;
        indices[indexOffset+1] = vertexOffset+1;
        indices[indexOffset+2] = vertexOffset+2;
        indices[indexOffset+3] = vertexOffset+2;
        indices[indexOffset+4] = vertexOffset+3;
        indices[indexOffset+5] = vertexOffset+0;

        return indexOffset;
    }

    void UIRenderSystem::onFrame(const Carrot::Render::Context& renderContext) {
        if (!rectanglePipelineResource.isReady()) {
            return;
        }
        if (!imagePipelineResource.isReady()) {
            return;
        }
        if (!testImage.isReady()) {
            return;
        }

        VulkanRenderer& renderer = renderContext.renderer;
        if (!testImageHandle) {
            testImageHandle = renderer.getMaterialSystem().createTextureHandle(testImage.get());
        }

        Clay_SetLayoutDimensions(Clay_Dimensions{.width = static_cast<float>(renderContext.pViewport->getWidth()), .height = static_cast<float>(renderContext.pViewport->getHeight())});

        // TODO: pointer state
        // TODO: scroll containers

        Clay_BeginLayout();

        Carrot::Render::Texture* pTestImage = testImage.get().get();

        CLAY(CLAY_ID("OuterContainer"), { .layout = { .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16 }, .backgroundColor = Clay_Color{250,250,255,255} }) {
            CLAY(CLAY_ID("SideBar"), {
                .layout = { .sizing = { .width = CLAY_SIZING_FIXED(300), .height = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16, .layoutDirection = CLAY_TOP_TO_BOTTOM },
                .backgroundColor = Clay_Color(255, 240, 20, 255)
            }) {
                CLAY(CLAY_ID("ProfilePictureOuter"), { .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } }, .backgroundColor = Clay_Color(255, 0, 0, 255) }) {
                    CLAY(CLAY_ID("ProfilePicture"), { .layout = { .sizing = { .width = CLAY_SIZING_FIXED(60), .height = CLAY_SIZING_FIXED(60) }}, .image = { .imageData = pTestImage } }) {}
                    CLAY_TEXT(CLAY_STRING("Clay - UI Library"), { .textColor = {255, 255, 255, 255}, .fontSize = 24 });
                }

                CLAY(CLAY_ID("MainContent"), { .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) } }, .backgroundColor = Clay_Color(255, 0, 255, 255) }) {}
            }
        }

        float dt = GetEngine().getCurrentFrameTime() - lastTime;
        Clay_RenderCommandArray renderCommands = Clay_EndLayout(dt);

        u32 vertexCount = 0;
        u32 indexCount = 0;
        // allocate geometry data
        for (i32 commandIndex = 0; commandIndex < renderCommands.length; commandIndex++) {
            const Clay_RenderCommand& command = renderCommands.internalArray[commandIndex];
            switch (command.commandType) {
                case CLAY_RENDER_COMMAND_TYPE_IMAGE:
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                    vertexCount += 4;
                    indexCount += 6;
                } break;

                    // TODO: others
            }
        }

        Carrot::Vector<ImGuiVertex> vertices;
        Carrot::Vector<u32> indices;
        vertices.ensureReserve(vertexCount);
        indices.ensureReserve(indexCount);

        struct CommandData {
            u32 indexOffset;
            i32 textureIndex = -1;
        };

        Carrot::Vector<CommandData> commandData;
        commandData.resize(renderCommands.length);

        // generate geometry
        for (i32 commandIndex = 0; commandIndex < renderCommands.length; commandIndex++) {
            const Clay_RenderCommand& command = renderCommands.internalArray[commandIndex];

            const float x = command.boundingBox.x;
            const float y = command.boundingBox.y;
            const float width = command.boundingBox.width;
            const float height = command.boundingBox.height;
            switch (command.commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                    const Clay_Color& bgColor = command.renderData.rectangle.backgroundColor;
                    glm::vec4 color{bgColor.r, bgColor.g, bgColor.b, bgColor.a};
                    const u32 r = static_cast<u32>(color.r) & 0xFF;
                    const u32 g = static_cast<u32>(color.g) & 0xFF;
                    const u32 b = static_cast<u32>(color.b) & 0xFF;
                    const u32 a = static_cast<u32>(color.a) & 0xFF;
                    const u32 u32Color = (a << 24) | (b << 16) | (g << 8) | (r);

                    commandData[commandIndex].indexOffset = generateQuadGeometry(x, y, width, height, glm::vec2{0.0f}, glm::vec2{1.0f}, u32Color, vertices, indices);
                } break;

                case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
                    const Clay_Color& bgColor = command.renderData.image.backgroundColor;
                    glm::vec4 color{bgColor.r, bgColor.g, bgColor.b, bgColor.a};
                    const u32 r = static_cast<u32>(color.r) & 0xFF;
                    const u32 g = static_cast<u32>(color.g) & 0xFF;
                    const u32 b = static_cast<u32>(color.b) & 0xFF;
                    const u32 a = static_cast<u32>(color.a) & 0xFF;
                    const u32 u32Color = (a << 24) | (b << 16) | (g << 8) | (r);

                    commandData[commandIndex].indexOffset = generateQuadGeometry(x, y, width, height, glm::vec2{0.0f}, glm::vec2{1.0f}, u32Color, vertices, indices);
                    commandData[commandIndex].textureIndex = testImageHandle->getSlot(); // TODO: not hardcoded
                } break;

                // TODO: others
            }
        }

        // upload geometry
        ResourceAllocator& allocator = GetResourceAllocator();
        Carrot::BufferAllocation vertexBufferAlloc = allocator.allocateDeviceBuffer(vertexCount * sizeof(ImGuiVertex), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer);
        Carrot::BufferAllocation indexBufferAlloc = allocator.allocateDeviceBuffer(indexCount * sizeof(u32), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer);
        vertexBufferAlloc.view.uploadForFrame(vertices.data(), vertices.bytes_size());
        indexBufferAlloc.view.uploadForFrame(indices.data(), indices.bytes_size());

        // dispatch draws
        struct DisplayRect {
            u32 textureIndex;
        };
        for (i32 commandIndex = 0; commandIndex < renderCommands.length; commandIndex++) {
            const Clay_RenderCommand& command = renderCommands.internalArray[commandIndex];
            const CommandData& dataForThisCommand = commandData[commandIndex];
            switch (command.commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                    // TODO: merge rectangles in single render packet for perf?
                    Carrot::Render::Packet& packet = renderer.makeRenderPacket(Render::PassEnum::UI, Render::PacketType::DrawIndexedInstanced, renderContext);
                    packet.vertexBuffer = vertexBufferAlloc.view;
                    packet.indexBuffer = indexBufferAlloc.view;
                    packet.instanceCount = 1;
                    packet.pipeline = rectanglePipelineResource.get();
                    packet.transparentGBuffer.zOrder = commandIndex; // to ensure objects are drawn on top of each others

                    Render::PacketCommand& packetCommand = packet.commands.emplace_back();
                    packetCommand.drawIndexedInstanced.indexCount = 6;
                    packetCommand.drawIndexedInstanced.firstIndex = dataForThisCommand.indexOffset;
                    packetCommand.drawIndexedInstanced.instanceCount = 1;

                    renderer.render(packet);
                } break;
                case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
                    Carrot::Render::Packet& packet = renderer.makeRenderPacket(Render::PassEnum::UI, Render::PacketType::DrawIndexedInstanced, renderContext);
                    packet.vertexBuffer = vertexBufferAlloc.view;
                    packet.indexBuffer = indexBufferAlloc.view;
                    packet.instanceCount = 1;
                    packet.pipeline = imagePipelineResource.get();
                    packet.transparentGBuffer.zOrder = commandIndex; // to ensure objects are drawn on top of each others

                    Render::PacketCommand& packetCommand = packet.commands.emplace_back();
                    packetCommand.drawIndexedInstanced.indexCount = 6;
                    packetCommand.drawIndexedInstanced.firstIndex = dataForThisCommand.indexOffset;
                    packetCommand.drawIndexedInstanced.instanceCount = 1;

                    auto& pushConstant = packet.addPushConstant("entryPointParams", vk::ShaderStageFlagBits::eFragment);
                    DisplayRect rect {
                        .textureIndex = static_cast<u32>(dataForThisCommand.textureIndex),
                    };
                    pushConstant.setData(rect);

                    renderer.render(packet);
                } break;

                    // TODO: others
            }
        }

        lastTime = GetEngine().getCurrentFrameTime();
    }

    std::unique_ptr<System> UIRenderSystem::duplicate(World& newOwner) const {
        return std::make_unique<UIRenderSystem>(newOwner);
    }
} // Carrot::ECS