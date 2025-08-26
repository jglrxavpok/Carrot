//
// Created by jglrxavpok on 01/05/2021.
//

#include "ParticleEditor.h"
#include <node_based/nodes/Arithmetics.hpp>
#include <node_based/nodes/Constants.hpp>
#include <node_based/nodes/Logics.hpp>
#include <node_based/nodes/BuiltinFunctions.h>
#include <node_based/nodes/Template.h>
#include <iostream>
#include <fstream>
#include <rapidjson/writer.h>
#include <rapidjson/filewritestream.h>
#include <filesystem>
#include <cstdlib>

#include "core/io/IO.h"
#include "core/utils/JSON.h"
#include "node_based/generators/ParticleShaderGenerator.h"
#include <core/data/ParticleBlueprint.h>
#include "engine/render/RenderGraph.h"
#include "engine/render/TextureRepository.h"

#include <spirv_glsl.hpp>
#include <nfd.h>
#include <engine/render/CameraBufferObject.h>
#include <engine/render/resources/ResourceAllocator.h>
#include <node_based/nodes/CommentNode.h>
#include <node_based/nodes/DefaultNodes.h>
#include <rapidjson/prettywriter.h>

namespace ed = ax::NodeEditor;
using namespace Fertilizer;

static std::uint32_t MaxPreviewParticles = 10000;

Peeler::ParticleEditor::ParticleEditor(Carrot::Engine& engine)
: Tools::ProjectMenuHolder(), engine(engine), settings("particle_editor"), templateEditor(engine),
updateGraph("UpdateEditor"), renderGraph("RenderEditor"), previewRenderGraph(), previewViewport(engine.createViewport(engine.getMainWindow()))
{
    previewRenderGraphBuilder = std::make_unique<Carrot::Render::GraphBuilder>(engine.getVulkanDriver(), engine.getMainWindow());
    struct TmpPass {
        Carrot::Render::FrameResource color;
    };

    auto& tmpPass = previewRenderGraphBuilder->addPass<TmpPass>("tmp-test",
    [this](Carrot::Render::GraphBuilder& builder, Carrot::Render::Pass<TmpPass>& pass, TmpPass& data) {
        vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
        data.color = builder.createRenderTarget("preview",
                                                vk::Format::eR8G8B8A8Unorm,
                                                vk::Extent3D{.width = 500, .height = 500, .depth = 1},
                                                vk::AttachmentLoadOp::eClear,
                                                clearColor,
                                                vk::ImageLayout::eColorAttachmentOptimal);

    },
    [this](const Carrot::Render::CompiledPass& pass, const Carrot::Render::Context& frame, const TmpPass& data, vk::CommandBuffer& cmds) {
        if(previewSystem) {
            auto& renderingPipeline = previewSystem->getRenderingPipeline();
            previewSystem->renderOpaqueGBuffer(pass, frame, cmds);
            previewSystem->renderTransparentGBuffer(pass, frame, cmds);
        }
    });

    previewColorTexture = tmpPass.getData().color;

    auto& composer = engine.getMainComposer();
    composer.add(tmpPass.getData().color/*, -0.25, 0.25, 1.0, 0.5, -0.5*/);

    previewRenderGraph = previewRenderGraphBuilder->compile();

    previewViewport.getCamera() = Carrot::Camera(90.0f, 1.0f, 0.001f, 10000.0f);
    previewViewport.getCamera().getPositionRef() = glm::vec3(2, 2, 3);
    previewViewport.getCamera().getTargetRef() = glm::vec3(0, 0, 2.5);

    settings.load();
    settings.save();

    settings.useMostRecent();

    Nodes::addParticleEditorNodesForRenderGraph(renderGraph);
    Nodes::addParticleEditorNodesForRenderGraph(templateEditor.getGraph());
    Nodes::addParticleEditorNodesForUpdateGraph(updateGraph);
    Nodes::addParticleEditorNodesForUpdateGraph(templateEditor.getGraph());

    ProjectMenuHolder::attachSettings(settings);

    if(settings.currentProject) {
        scheduleLoad(settings.currentProject.value());
    }

    std::filesystem::path testPath = "resources/node_templates/vec2length.json";
    templateEditor.scheduleLoad(testPath);

    templateEditor.open();

    templateEditor.getGraph().setImGuiTexturesProvider(this);
    renderGraph.setImGuiTexturesProvider(this);
    updateGraph.setImGuiTexturesProvider(this);
}

void Peeler::ParticleEditor::updateUpdateGraph(Carrot::Render::Context renderContext) {
    updateGraph.draw();
}

void Peeler::ParticleEditor::updateRenderGraph(Carrot::Render::Context renderContext) {
    renderGraph.draw();
}

void Peeler::ParticleEditor::saveToFile(std::filesystem::path path) {
    rapidjson::Document document;
    document.SetObject();

    document.AddMember("update_graph", updateGraph.toJSON(document), document.GetAllocator());
    document.AddMember("render_graph", renderGraph.toJSON(document), document.GetAllocator());

    FILE* fp = fopen(path.string().c_str(), "wb"); // non-Windows use "w"

    char writeBuffer[65536];
    rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

    rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);

    document.Accept(writer);
    fclose(fp);

    Carrot::JSON::clearReferences();

    updateGraph.resetChangeFlag();
    renderGraph.resetChangeFlag();
    reloadPreview();
}

void Peeler::ParticleEditor::generateParticleFile(const std::filesystem::path& filename) {
    std::unordered_set<Carrot::UUID> activeLinks; // not read from during generation (only used for visualisation)
    auto updateExpressions = updateGraph.generateExpressionsFromTerminalNodes(activeLinks);
    auto fragmentExpressions = renderGraph.generateExpressionsFromTerminalNodes(activeLinks);
    ParticleShaderGenerator updateGenerator(ParticleShaderMode::Compute, getCurrentProjectName());
    ParticleShaderGenerator fragmentGenerator(ParticleShaderMode::Fragment, getCurrentProjectName());

    auto computeShader = updateGenerator.compileToSPIRV(updateExpressions);
    auto fragmentShader = fragmentGenerator.compileToSPIRV(fragmentExpressions);
    bool isOpaque = false; // TODO: determine via render graph

    Carrot::ParticleBlueprint blueprint(std::move(computeShader), std::move(fragmentShader), isOpaque);

    Carrot::IO::writeFile(filename.string(), [&](std::ostream& out) {
        out << blueprint;
    });
}

void Peeler::ParticleEditor::onFrame(const Carrot::Render::Context& renderContext) {

    // TODO: move to init
    {
        ed::Style& style = ed::GetStyle();

        // colors taken from Blender's node editor
        style.Colors[ed::StyleColor_Bg] = ImColor(29, 29, 29, 255);
        style.Colors[ed::StyleColor_Grid] = ImColor(40, 40, 40, 255);

        style.Colors[ed::StyleColor_NodeBorder] = ImColor(18, 18, 18, 255);
        style.Colors[ed::StyleColor_SelNodeBorder] = ImColor(237, 237, 237, 255);
        style.Colors[ed::StyleColor_HovNodeBorder] = style.Colors[ed::StyleColor_NodeBorder];
        style.Colors[ed::StyleColor_NodeBg] = ImColor(48, 48, 48, 255);

        style.Colors[ed::StyleColor_GroupBg] = ImColor(5, 5, 5, 255);
        style.Colors[ed::StyleColor_GroupBorder] = style.Colors[ed::StyleColor_NodeBorder];

        style.NodePadding.x = 0;
        style.NodePadding.z = 0;
    }

    if(renderContext.pViewport == &previewViewport) {
        if(previewSystem) {
            previewSystem->onFrame(renderContext);

            engine.getVulkanDriver().performSingleTimeGraphicsCommands([&](vk::CommandBuffer& cmds) {
                previewRenderGraph->execute(renderContext, cmds);
            });
        }

    } else if (renderContext.pViewport == &GetEngine().getMainViewport()) {
        float menuBarHeight = 0;
        if(ImGui::BeginMenuBar()) {
            handleShortcuts(renderContext);
            if(ImGui::BeginMenu("Project")) {
                drawProjectMenu();

                if(ImGui::MenuItem("Export...")) {
                    nfdchar_t* savePath;

                    // prepare filters for the dialog
                    nfdfilteritem_t filterItem[1] = {{"Particle", "particle"}};

                    // show the dialog
                    std::string defaultFilename = getCurrentProjectName()+".particle";
                    nfdresult_t result = NFD_SaveDialog(&savePath, filterItem, 1, nullptr, defaultFilename.c_str());
                    if (result == NFD_OKAY) {
                        saveToFile(savePath);
                        std::filesystem::path path = savePath;
                        generateParticleFile(path);
                        // remember to free the memory (since NFD_OKAY is returned)
                        NFD_FreePath(savePath);
                    } else if (result == NFD_CANCEL) {
                        // no op
                    } else {
                        std::string msg = "Error: ";
                        msg += NFD_GetError();
                        throw std::runtime_error(msg);
                    }
                }

                ImGui::EndMenu();
            }

            if(ImGui::BeginMenu("Tests")) {
                if(ImGui::MenuItem("Print tree")) {
                    std::unordered_set<Carrot::UUID> activeLinks;
                    auto expressions = updateGraph.generateExpressionsFromTerminalNodes(activeLinks);
                    for(const auto& expr : expressions) {
                        std::cout << expr->toString() << '\n';
                    }
                    std::cout << std::endl;
                }

                ImGui::EndMenu();
            }

            menuBarHeight = ImGui::GetWindowSize().y;
            ImGui::EndMenuBar();
        }

        /*auto& viewport = *ImGui::GetMainViewport();
        ImGui::SetNextWindowViewport(viewport.ID);
        ImGui::SetNextWindowPos(ImVec2(viewport.Pos.x, viewport.Pos.y+menuBarHeight));
        ImGui::SetNextWindowSize(ImVec2(engine.getVulkanDriver().getFinalRenderSize(engine.getMainWindow()).width,
                                        engine.getVulkanDriver().getFinalRenderSize(engine.getMainWindow()).height - menuBarHeight));*/
        focusedGraph = Graph::None;
        if(ImGui::BeginTabBar("ParticleEditorTabs")) {
            if(ImGui::BeginTabItem("Update##tab particle editor")) {
                updateUpdateGraph(renderContext);
                focusedGraph = Graph::Update;
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Render##tab particle editor")) {
                updateRenderGraph(renderContext);
                focusedGraph = Graph::Render;
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        templateEditor.onFrame(renderContext);

        ProjectMenuHolder::onFrame(renderContext);

        UIParticlePreview(renderContext);
    }
}

void Peeler::ParticleEditor::UIParticlePreview(Carrot::Render::Context renderContext) {
    ImVec2 previewSize { 200, 200 };
    ImGui::SetNextWindowPos(ImVec2(WINDOW_WIDTH/2 - previewSize.x/2, WINDOW_HEIGHT - previewSize.y/2), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(previewSize, ImGuiCond_FirstUseEver);
    if(ImGui::Begin("preview##preview", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar)) {
        auto windowSize = ImGui::GetContentRegionAvail();
        auto& texture = renderContext.renderer.getVulkanDriver().getResourceRepository().getTexture(previewColorTexture, renderContext.frameIndex);
        ImGui::Image(texture.getImguiID(), windowSize);
        // TODO camera controls
    }
    ImGui::End();
}

void Peeler::ParticleEditor::reloadPreview() {
    std::unordered_set<Carrot::UUID> activeLinks;
    auto colorActiveLinks = [&](EditorGraph& graph) {
        graph.removeColoredLinks();
        for (const auto& uuid : activeLinks) {
            graph.addColoredLink(uuid);
        }
    };

    auto updateExpressions = updateGraph.generateExpressionsFromTerminalNodes(activeLinks);
    colorActiveLinks(updateGraph);

    activeLinks.clear();
    auto fragmentExpressions = renderGraph.generateExpressionsFromTerminalNodes(activeLinks);
    colorActiveLinks(renderGraph);

    ParticleShaderGenerator updateGenerator(ParticleShaderMode::Compute, "ParticleEditor-Preview");
    ParticleShaderGenerator fragmentGenerator(ParticleShaderMode::Fragment, "ParticleEditor-Preview");

    auto computeShader = updateGenerator.compileToSPIRV(updateExpressions);
    auto fragmentShader = fragmentGenerator.compileToSPIRV(fragmentExpressions);
    bool isOpaque = false; // TODO: determine via render graph

    previewBlueprint = std::make_unique<Carrot::RenderableParticleBlueprint>(std::move(computeShader), std::move(fragmentShader), isOpaque);
    previewSystem = std::make_unique<Carrot::ParticleSystem>(engine, *previewBlueprint, MaxPreviewParticles);

    auto& emitter = *previewSystem->createEmitter();
    emitter.setRate(100.0f);
}

void Peeler::ParticleEditor::tick(double deltaTime) {
    updateGraph.tick(deltaTime);
    renderGraph.tick(deltaTime);

    if (reloadCooldown > 0) {
        reloadCooldown -= deltaTime;
    } else {
        if (updateGraph.getLastChangeTime() > lastReloadTime || renderGraph.getLastChangeTime() > lastReloadTime) {
            reloadPreview();
            lastReloadTime = std::chrono::steady_clock::now();
            reloadCooldown = 0.5f;
        }
    }

    if(previewSystem) {
        previewSystem->tick(deltaTime);
    }

    hasUnsavedChanges = updateGraph.hasChanges() || renderGraph.hasChanges();
}

Peeler::ParticleEditor::~ParticleEditor() {
}

void Peeler::ParticleEditor::performLoad(std::filesystem::path fileToOpen) {
    if(fileToOpen == EmptyProject) {
        updateGraph.clear();
        renderGraph.clear();
        hasUnsavedChanges = false;
        settings.currentProject.reset();
        return;
    }
    rapidjson::Document description;
    description.Parse(Carrot::IO::readFileAsText(fileToOpen.string()).c_str());

    updateGraph.loadFromJSON(description["update_graph"]);
    renderGraph.loadFromJSON(description["render_graph"]);
    settings.currentProject = fileToOpen;

    settings.addToRecentProjects(fileToOpen);
    hasUnsavedChanges = false;
    reloadPreview();
}

void Peeler::ParticleEditor::clear() {
    updateGraph.clear();
    renderGraph.clear();
    hasUnsavedChanges = false;
    reloadPreview();
}

bool Peeler::ParticleEditor::showUnsavedChangesPopup() {
    return hasUnsavedChanges;
}

void Peeler::ParticleEditor::onSwapchainImageCountChange(size_t newCount) {
    previewRenderGraph->onSwapchainImageCountChange(newCount);
}

void Peeler::ParticleEditor::onSwapchainSizeChange(Carrot::Window& w, int newWidth, int newHeight) {
    previewRenderGraph->onSwapchainSizeChange(w, newWidth, newHeight);
}

ImTextureID Peeler::ParticleEditor::getExpressionType(const Carrot::ExpressionType& type) {
    auto iter = expressionTypesTextures.find(type);
    if (iter == expressionTypesTextures.end()) {
        return nullptr;
    }
    return iter->second->getImguiID();
}


void Peeler::ParticleEditor::onCutShortcut(const Carrot::Render::Context& frame) {
    switch(focusedGraph) {
        case Graph::Render:
            renderGraph.onCutShortcut();
            break;

        case Graph::Update:
            updateGraph.onCutShortcut();
            break;
    }
    templateEditor.onCutShortcut(frame);
}

void Peeler::ParticleEditor::onCopyShortcut(const Carrot::Render::Context& frame) {
    switch(focusedGraph) {
        case Graph::Render:
            renderGraph.onCopyShortcut();
            break;

        case Graph::Update:
            updateGraph.onCopyShortcut();
            break;
    }
    templateEditor.onCopyShortcut(frame);
}

void Peeler::ParticleEditor::onPasteShortcut(const Carrot::Render::Context& frame) {
    switch(focusedGraph) {
        case Graph::Render:
            renderGraph.onPasteShortcut();
            break;

        case Graph::Update:
            updateGraph.onPasteShortcut();
            break;
    }
    templateEditor.onPasteShortcut(frame);
}

void Peeler::ParticleEditor::onDuplicateShortcut(const Carrot::Render::Context& frame) {
    switch(focusedGraph) {
        case Graph::Render:
            renderGraph.onDuplicateShortcut();
            break;

        case Graph::Update:
            updateGraph.onDuplicateShortcut();
            break;
    }
    templateEditor.onDuplicateShortcut(frame);
}
