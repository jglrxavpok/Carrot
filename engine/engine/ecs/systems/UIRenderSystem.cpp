//
// Created by jglrxavpok on 13/06/2026.
//

#include "UIRenderSystem.h"

#include <clay.h>
#include <engine/Engine.h>
#include <engine/ecs/components/ui/UIBoxComponent.h>
#include <engine/ecs/components/ui/UICanvasComponent.h>
#include <engine/render/resources/ResourceAllocator.h>

// UIRenderSystem ignores transform propagation for scale:
// Because a parent container can have "fit" as its sizing policy, changing the size of a child changes the size of the parent,
//  leading to a feedback loop that does not run away, but makes it hard to understand what is being resized and how.
// Therefore scales are always taken from the local transform which seems to work well enough and intuitively inside the editor
namespace Carrot::ECS {
    UIRenderSystem::UIRenderSystem(World& world)
                : RenderSystem<TransformComponent, Carrot::UI::UICanvasComponent>(world) {
        rectanglePipelineResource = AsyncResource<Pipeline, false>(GetAssetServer().loadPipelineTask("resources/pipelines/ui/rectangle.pipeline"));
        imagePipelineResource = AsyncResource<Pipeline, false>(GetAssetServer().loadPipelineTask("resources/pipelines/ui/image.pipeline"));
        testImage = AsyncResource<Render::Texture, false>(GetAssetServer().loadTextureTask("resources/icon128.ktx2"));

        rectangleComponentSignature.addComponent<TransformComponent>();
        rectangleComponentSignature.addComponent<UI::UIBoxComponent>();

        pFont = Carrot::makeUnique<Render::Font>(Allocator::getDefault(), GetRenderer(), "resources/fonts/Roboto-Medium.ttf");
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

    static glm::vec2 convertPositionToClay(const glm::vec2& carrotPosition, const glm::vec2& canvasSize) {
        return carrotPosition * canvasSize;
    }

    static glm::vec2 convertPositionToCarrot(const glm::vec2& clayPosition, const glm::vec2& canvasSize) {
        return clayPosition / canvasSize;
    }

    static Clay_ElementDeclaration toElement(const TransformComponent& transform, const UI::UIBoxComponent& box, const glm::vec2& viewportSize) {
        Clay_ElementDeclaration decl{};
        // TODO: layout

        decl.layout.childGap = box.childGap;
        decl.layout.padding.left = box.padding[0];
        decl.layout.padding.right = box.padding[1];
        decl.layout.padding.bottom = box.padding[2];
        decl.layout.padding.top = box.padding[3];
        using namespace UI;
        auto toClay = [&](const UI::SizeType& size, int axisIndex) {
            if (const Fit* pFit = std::get_if<Fit>(&size)) {
                return CLAY_SIZING_FIT(pFit->range.min, pFit->range.max);
            } else if (const Grow* pGrow = std::get_if<Grow>(&size)) {
                return CLAY_SIZING_GROW(pGrow->range.min, pGrow->range.max);
            } else if (const Percent* pPercent = std::get_if<Percent>(&size)) {
                return CLAY_SIZING_PERCENT(pPercent->ratio);
            } else if (const Fixed* pFixed = std::get_if<Fixed>(&size)) {
                return CLAY_SIZING_FIXED(convertPositionToClay(transform.localTransform.scale.xy(), viewportSize)[axisIndex]);
            } else {
                TODO;
            }
        };
        decl.layout.sizing.width = toClay(box.width, 0);
        decl.layout.sizing.height = toClay(box.height, 1);

        if (box.image) {
            decl.image.imageData = box.image.get();

            // x255 to match with Clay convention, even if it is divided by 255 before sending to shader
            decl.userData = (void*)&box.color;
        } else {
            // x255 to match with Clay convention, even if it is divided by 255 before sending to shader
            decl.backgroundColor.r = box.color.r*255;
            decl.backgroundColor.g = box.color.g*255;
            decl.backgroundColor.b = box.color.b*255;
            decl.backgroundColor.a = box.color.a*255;
        }

        return decl;
    }

    static Clay_String toClayString(std::string_view view) {
        return Clay_String {
            .isStaticallyAllocated = false,
            .length = static_cast<i32>(view.size()),
            .chars = view.data(),
        };
    }

    static glm::vec4 convertColor(const Clay_Color& c) {
        return glm::vec4 {
            c.r / 255.0f,
            c.g / 255.0f,
            c.b / 255.0f,
            c.a / 255.0f,
        };
    }

    Carrot::Vector<UIRenderSystem::LayoutResult> UIRenderSystem::translateToClay(const Render::Context& renderContext, float dt) {
        Carrot::Render::Texture* pTestImage = testImage.get().get();

        Carrot::Vector<LayoutResult> results;
        // a bit awkward because the ECS is not meant for hierarchy traversal
        forEachEntity([&](Carrot::ECS::Entity& entity, Carrot::ECS::TransformComponent& transform, Carrot::UI::UICanvasComponent& canvas) {
            Clay_BeginLayout();

            // TODO: make it work in 3D space
            const glm::vec2 viewportSize = renderContext.pViewport->getSizef();
            std::function<void(Carrot::ECS::Entity& potentialUIElement)> recurse = [this, &recurse, &viewportSize](Carrot::ECS::Entity& potentialUIElement) {
                auto pTransform = potentialUIElement.getComponent<TransformComponent>();

                if (Memory::OptionalRef<UI::UIBoxComponent> boxComp = potentialUIElement.getComponent<UI::UIBoxComponent>(); boxComp && pTransform && potentialUIElement.isVisible()) {
                    const Carrot::UUID& uuid = potentialUIElement.getID();
                    auto [iter, wasNew] = id2stringStorage.emplace(uuid, uuid.toString());
                    Clay_ElementDeclaration config = toElement(pTransform, boxComp, viewportSize);
                    CLAY(CLAY_SID(toClayString(iter->second)), config) {
                        for (auto& child : potentialUIElement.getChildren(ShouldRecurse::NoRecursion)) {
                            recurse(child);
                        }
                    }
                } else {
                    for (auto& child : potentialUIElement.getChildren(ShouldRecurse::NoRecursion)) {
                        recurse(child);
                    }
                }
            };

            recurse(entity);

            /*CLAY(CLAY_ID("OuterContainer"), { .layout = { .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16 }, .backgroundColor = Clay_Color{250,250,255,255} }) {
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
            }*/
            Clay_RenderCommandArray renderCommands = Clay_EndLayout(dt);
            results.emplaceBack(LayoutResult {
                .clayCommands = renderCommands,
                .canvasSize = viewportSize,
                .transform = transform.toTransformMatrix(),
                .inWorld = canvas.inWorld,
            });

            std::function<void(Carrot::ECS::Entity& potentialUIElement, glm::vec2 accumulatedTranslation)> recurseUpdateTransform = [&](Carrot::ECS::Entity& potentialUIElement, glm::vec2 accumulatedTranslation) {
                auto pTransform = potentialUIElement.getComponent<TransformComponent>();

                if (Memory::OptionalRef<UI::UIBoxComponent> boxComp = potentialUIElement.getComponent<UI::UIBoxComponent>(); boxComp && pTransform && potentialUIElement.isVisible()) {
                    const Carrot::UUID& uuid = potentialUIElement.getID();
                    auto iter = id2stringStorage.find(uuid);
                    verify(iter != id2stringStorage.end(), "Logic error: uuid was not in id2stringStorage but the item is in the hierarchy?");

                    const Clay_ElementData data = Clay_GetElementData(CLAY_SID(toClayString(iter->second)));
                    verify(data.found, "Logic error: item not found in Clay hierarchy, but was inside Carrot hierarchy");
                    Carrot::Math::Transform newTransform{};

                    glm::vec2 position = convertPositionToCarrot(glm::vec2{data.boundingBox.x, data.boundingBox.y}, viewportSize);
                    glm::vec2 size = convertPositionToCarrot(glm::vec2{data.boundingBox.width, data.boundingBox.height}, viewportSize);

                    //size /= 2.0f;
                    position += size/2.0f; // center
                    //position += 0.5f;
                    position.y *= -1.0f;

                    newTransform.position.x = -accumulatedTranslation.x + position.x;
                    newTransform.position.y = -accumulatedTranslation.y + position.y;
                    accumulatedTranslation += position;
                    newTransform.position.z = 0.0f;
                    newTransform.scale.x = size.x;
                    newTransform.scale.y = size.y;
                    newTransform.scale.z = 0.01f;
                    pTransform->localTransform = newTransform;

                    for (auto& child : potentialUIElement.getChildren(ShouldRecurse::NoRecursion)) {
                        recurseUpdateTransform(child, accumulatedTranslation);
                    }
                } else {
                    for (auto& child : potentialUIElement.getChildren(ShouldRecurse::NoRecursion)) {
                        recurseUpdateTransform(child, accumulatedTranslation);
                    }
                }
            };

            recurseUpdateTransform(entity, {0,0});
        });

        return results;
    }

    void UIRenderSystem::onFrame(const Carrot::Render::Context& renderContext) {
        id2stringStorage.clear();
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

        float dt = GetEngine().getCurrentFrameTime() - lastTime;
        Carrot::Vector<LayoutResult> results = translateToClay(renderContext, dt);

        for (const LayoutResult& result : results) {
            const Clay_RenderCommandArray& renderCommands = result.clayCommands;

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
                Render::RenderableText* pText = nullptr;
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
                        glm::vec4 tintColor = *static_cast<glm::vec4*>(command.userData);
                        const u32 r = static_cast<u32>(tintColor.r*255) & 0xFF;
                        const u32 g = static_cast<u32>(tintColor.g*255) & 0xFF;
                        const u32 b = static_cast<u32>(tintColor.b*255) & 0xFF;
                        const u32 a = static_cast<u32>(tintColor.a*255) & 0xFF;
                        const u32 u32Color = (a << 24) | (b << 16) | (g << 8) | (r);

                        commandData[commandIndex].indexOffset = generateQuadGeometry(x, y, width, height, glm::vec2{0.0f}, glm::vec2{1.0f}, u32Color, vertices, indices);
                        commandData[commandIndex].textureIndex = testImageHandle->getSlot(); // TODO: not hardcoded
                    } break;

                    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                        const Clay_StringSlice slice = command.renderData.text.stringContents;
                        std::string str { slice.chars, static_cast<std::size_t>(slice.length) };
                        RenderedTextKey key {
                            .text = str,
                            .textSize = command.renderData.text.fontSize,
                        };
                        auto [iter, wasNew] = texts.try_emplace(key);
                        if (wasNew) {
                            iter->second = pFont->bake(Carrot::toU32String(str), command.renderData.text.fontSize, Render::TextAlignment::LeftOrTop, Render::TextAlignment::LeftOrTop);
                        }
                        commandData[commandIndex].pText = &iter->second;
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
                glm::mat4 transform;
                u32 textureIndex;
                u32 drawFlags;

                glm::vec2 scissorOffset;
                glm::vec2 scissorExtent;

                glm::vec4 overlayColor;
            };

            glm::vec4 currentOverlayColor{1,1,1,1};
            std::optional<vk::Rect2D> scissor;
            for (i32 commandIndex = 0; commandIndex < renderCommands.length; commandIndex++) {
                const Clay_RenderCommand& command = renderCommands.internalArray[commandIndex];
                const CommandData& dataForThisCommand = commandData[commandIndex];

                Carrot::Render::Packet& packet = renderer.makeRenderPacket(Render::PassEnum::UI, Render::PacketType::DrawIndexedInstanced, renderContext);
                packet.vertexBuffer = vertexBufferAlloc.view;
                packet.indexBuffer = indexBufferAlloc.view;
                packet.instanceCount = 1;
                packet.transparentGBuffer.zOrder = commandIndex; // to ensure objects are drawn on top of each others

                auto& pushConstant = packet.addPushConstant("entryPointParams", vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
                DisplayRect rect {
                    .transform = result.transform,
                    .textureIndex = static_cast<u32>(dataForThisCommand.textureIndex),
                    .overlayColor = currentOverlayColor,
                };
                rect.drawFlags = 0;

                if (result.inWorld) {
                    rect.drawFlags |= 1<<0;
                }

                if (scissor.has_value()) {
                    rect.drawFlags |= 1<<1;
                    rect.scissorOffset = glm::vec2{scissor->offset.x, scissor->offset.y};
                    rect.scissorExtent = glm::vec2{scissor->extent.width, scissor->extent.height};
                }

                switch (command.commandType) {
                    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                        // TODO: merge rectangles in single render packet for perf?
                        packet.pipeline = rectanglePipelineResource.get();

                        Render::PacketCommand& packetCommand = packet.commands.emplace_back();
                        packetCommand.drawIndexedInstanced.indexCount = 6;
                        packetCommand.drawIndexedInstanced.firstIndex = dataForThisCommand.indexOffset;
                        packetCommand.drawIndexedInstanced.instanceCount = 1;
                    } break;
                    case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
                        packet.pipeline = imagePipelineResource.get();

                        Render::PacketCommand& packetCommand = packet.commands.emplace_back();
                        packetCommand.drawIndexedInstanced.indexCount = 6;
                        packetCommand.drawIndexedInstanced.firstIndex = dataForThisCommand.indexOffset;
                        packetCommand.drawIndexedInstanced.instanceCount = 1;
                    } break;

                    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                        verify(dataForThisCommand.pText, "Missing rendered text");
                        dataForThisCommand.pText->getColor() = convertColor(command.renderData.text.textColor);
                        Math::Transform transform;
                        glm::vec2 pos = convertPositionToCarrot(glm::vec2{command.boundingBox.x, command.boundingBox.y+command.boundingBox.height}, result.canvasSize);
                        transform.position.x = pos.x*2-1;
                        transform.position.y = pos.y*2-1;
                        transform.position.z = 0;

                        transform.scale.x = 1;
                        transform.scale.y = -1;
                        transform.scale.z = 1;
                        dataForThisCommand.pText->getTransform() = result.transform * transform.toTransformMatrix();

                        dataForThisCommand.pText->renderInUI(renderContext, commandIndex);
                    } continue;

                    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
                        scissor.emplace();
                        scissor->offset.x = command.boundingBox.x;
                        scissor->offset.y = command.boundingBox.y;
                        scissor->extent.width = command.boundingBox.width;
                        scissor->extent.height = command.boundingBox.height;
                        continue; // no rendering

                    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                        scissor.reset();
                        continue; // no rendering

                    case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
                        currentOverlayColor = convertColor(command.renderData.overlayColor.color);
                        continue;

                    case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
                        currentOverlayColor = glm::vec4{1,1,1,1};
                        continue;

                    default:
                        continue; // skip this command
                        // TODO: others
                }

                pushConstant.setData(rect);
                renderer.render(packet);
            }
        }

        lastTime = GetEngine().getCurrentFrameTime();
    }

    std::unique_ptr<System> UIRenderSystem::duplicate(World& newOwner) const {
        return std::make_unique<UIRenderSystem>(newOwner);
    }
} // Carrot::ECS