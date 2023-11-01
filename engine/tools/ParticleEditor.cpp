//
// Created by jglrxavpok on 01/05/2021.
//

#include "ParticleEditor.h"
#include "nodes/Arithmetics.hpp"
#include "nodes/Constants.hpp"
#include "nodes/Logics.hpp"
#include "nodes/BuiltinFunctions.h"
#include "nodes/Template.h"
#include <iostream>
#include <fstream>
#include <rapidjson/writer.h>
#include <rapidjson/filewritestream.h>
#include <filesystem>
#include <cstdlib>

#include "core/io/IO.h"
#include "core/utils/JSON.h"
#include "tools/generators/ParticleShaderGenerator.h"
#include "engine/render/particles/ParticleBlueprint.h"
#include "engine/render/RenderGraph.h"
#include "engine/render/TextureRepository.h"

#include <spirv_glsl.hpp>
#include <nfd.h>
#include <engine/render/CameraBufferObject.h>
#include <engine/render/resources/ResourceAllocator.h>
#include <rapidjson/prettywriter.h>

namespace ed = ax::NodeEditor;

static std::uint32_t MaxPreviewParticles = 10000;

Tools::ParticleEditor::ParticleEditor(Carrot::Engine& engine)
: Tools::ProjectMenuHolder(), engine(engine), settings("particle_editor"), templateEditor(engine),
updateGraph(engine, "UpdateEditor"), renderGraph(engine, "RenderEditor"), previewRenderGraph(), previewViewport(engine.createViewport())
{
    previewRenderGraphBuilder = std::make_unique<Carrot::Render::GraphBuilder>(engine.getVulkanDriver());
    struct TmpPass {
        Carrot::Render::FrameResource color;
    };

    auto& tmpPass = previewRenderGraphBuilder->addPass<TmpPass>("tmp-test",
    [this](Carrot::Render::GraphBuilder& builder, Carrot::Render::Pass<TmpPass>& pass, TmpPass& data) {
        vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
        data.color = builder.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                vk::Extent3D{.width = 500, .height = 500, .depth = 1},
                                                vk::AttachmentLoadOp::eClear,
                                                clearColor,
                                                vk::ImageLayout::eColorAttachmentOptimal);

    },
    [this](const Carrot::Render::CompiledPass& pass, const Carrot::Render::Context& frame, const TmpPass& data, vk::CommandBuffer& cmds) {
        if(previewSystem) {
            auto& renderingPipeline = previewSystem->getRenderingPipeline();
            previewSystem->renderOpaqueGBuffer(pass.getRenderPass(), frame, cmds);
            previewSystem->renderTransparentGBuffer(pass.getRenderPass(), frame, cmds);
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

    {
        NodeLibraryMenuScope s1("Operators", &updateGraph);
        NodeLibraryMenuScope s2("Operators", &renderGraph);
        NodeLibraryMenuScope s3("Operators", &templateEditor.getGraph());
        addCommonOperators(updateGraph);
        addCommonOperators(renderGraph);
        addCommonOperators(templateEditor.getGraph());
    }
    {
        NodeLibraryMenuScope s1("Logic", &updateGraph);
        NodeLibraryMenuScope s2("Logic", &renderGraph);
        NodeLibraryMenuScope s3("Logic", &templateEditor.getGraph());
        addCommonLogic(updateGraph);
        addCommonLogic(renderGraph);
        addCommonLogic(templateEditor.getGraph());
    }

    {
        NodeLibraryMenuScope s1("Functions", &updateGraph);
        NodeLibraryMenuScope s2("Functions", &renderGraph);
        NodeLibraryMenuScope s3("Functions", &templateEditor.getGraph());
        addCommonMath(updateGraph);
        addCommonMath(renderGraph);
        addCommonMath(templateEditor.getGraph());
    }

    {
        NodeLibraryMenuScope s1("Inputs", &updateGraph);
        NodeLibraryMenuScope s2("Inputs", &renderGraph);
        NodeLibraryMenuScope s3("Inputs", &templateEditor.getGraph());
        addCommonInputs(updateGraph);
        addCommonInputs(renderGraph);
        addCommonInputs(templateEditor.getGraph());
    }

    {
        NodeLibraryMenuScope s1("Update Inputs", &updateGraph);
        NodeLibraryMenuScope s2("Render Inputs", &renderGraph);
        NodeLibraryMenuScope s3("Update/Render Inputs", &templateEditor.getGraph());
        updateGraph.addVariableToLibrary<VariableNodeType::GetDeltaTime>();
        templateEditor.getGraph().addVariableToLibrary<VariableNodeType::GetDeltaTime>();

        renderGraph.addVariableToLibrary<VariableNodeType::GetFragmentPosition>();
        templateEditor.getGraph().addVariableToLibrary<VariableNodeType::GetFragmentPosition>();
    }
    {
        NodeLibraryMenuScope s1("Update Outputs", &updateGraph);
        updateGraph.addToLibrary<TerminalNodeType::SetVelocity>();
        updateGraph.addToLibrary<TerminalNodeType::SetSize>();

        NodeLibraryMenuScope s2("Render Outputs", &renderGraph);
        renderGraph.addToLibrary<TerminalNodeType::SetOutputColor>();
        renderGraph.addToLibrary<TerminalNodeType::DiscardPixel>();

        NodeLibraryMenuScope s3("Render/Update Outputs", &templateEditor.getGraph());
        templateEditor.getGraph().addToLibrary<TerminalNodeType::SetOutputColor>();
        templateEditor.getGraph().addToLibrary<TerminalNodeType::DiscardPixel>();
        templateEditor.getGraph().addToLibrary<TerminalNodeType::SetVelocity>();
        templateEditor.getGraph().addToLibrary<TerminalNodeType::SetSize>();
    }

    renderGraph.addTemplatesToLibrary();
    updateGraph.addTemplatesToLibrary();
    renderGraph.addTemplateSupport();
    updateGraph.addTemplateSupport();

    ProjectMenuHolder::attachSettings(settings);

    if(settings.currentProject) {
        scheduleLoad(settings.currentProject.value());
    }

    std::filesystem::path testPath = "resources/node_templates/vec2length.json";
    templateEditor.scheduleLoad(testPath);

    templateEditor.open();
}

void Tools::ParticleEditor::addCommonInputs(Tools::EditorGraph& graph) {
    graph.addToLibrary<FloatConstantNode>("float_constant", "Float Constant");
    graph.addToLibrary<BoolConstantNode>("bool_constant", "Bool Constant");
    graph.addVariableToLibrary<VariableNodeType::GetSize>();
    graph.addVariableToLibrary<VariableNodeType::GetLife>();
    graph.addVariableToLibrary<VariableNodeType::GetParticleIndex>();
    graph.addVariableToLibrary<VariableNodeType::GetEmissionID>();
    graph.addVariableToLibrary<VariableNodeType::GetVelocity>();
    graph.addVariableToLibrary<VariableNodeType::GetPosition>();
}

void Tools::ParticleEditor::addCommonOperators(Tools::EditorGraph& graph) {
    graph.addToLibrary<AddNode>("add", "Add");
    graph.addToLibrary<SubNode>("sub", "Subtract");
    graph.addToLibrary<MultNode>("mult", "Multiply");
    graph.addToLibrary<DivNode>("div", "Divide");
    graph.addToLibrary<ModNode>("mod", "Modulus");
    graph.addToLibrary<MinNode>("min", "Minimum");
    graph.addToLibrary<MaxNode>("max", "Maximum");
}

void Tools::ParticleEditor::addCommonMath(Tools::EditorGraph& graph) {
    graph.addToLibrary<CosNode>("cos", "Cos");
    graph.addToLibrary<SinNode>("sin", "Sin");
    graph.addToLibrary<TanNode>("tan", "Tan");
    graph.addToLibrary<ExpNode>("exp", "Exp");
    graph.addToLibrary<AbsNode>("abs", "Abs");
    graph.addToLibrary<LogNode>("log", "Log");
    graph.addToLibrary<SqrtNode>("sqrt", "Sqrt");
}

void Tools::ParticleEditor::addCommonLogic(Tools::EditorGraph& graph) {
    graph.addToLibrary<LessNode>("float_less", "Less");
    graph.addToLibrary<LessOrEqualsNode>("float_less_or_equals", "Less or Equals");
    graph.addToLibrary<GreaterNode>("float_greater", "Greater");
    graph.addToLibrary<GreaterOrEqualsNode>("float_greater_or_equals", "Greater or Equals");
    graph.addToLibrary<EqualsNode>("float_equals", "Equals");
    graph.addToLibrary<NotEqualsNode>("float_not_equals", "Not Equals");

    graph.addToLibrary<AndNode>("and", "And");
    graph.addToLibrary<OrNode>("or", "Or");
    graph.addToLibrary<XorNode>("xor", "Xor");
    graph.addToLibrary<NegateBoolNode>("not", "Not");
}

void Tools::ParticleEditor::updateUpdateGraph(Carrot::Render::Context renderContext) {
    updateGraph.onFrame(renderContext);
}

void Tools::ParticleEditor::updateRenderGraph(Carrot::Render::Context renderContext) {
    renderGraph.onFrame(renderContext);
}

void Tools::ParticleEditor::saveToFile(std::filesystem::path path) {
    FILE* fp = fopen(path.string().c_str(), "wb"); // non-Windows use "w"

    char writeBuffer[65536];
    rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

    rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);

    rapidjson::Document document;
    document.SetObject();

    document.AddMember("update_graph", updateGraph.toJSON(document), document.GetAllocator());
    document.AddMember("render_graph", renderGraph.toJSON(document), document.GetAllocator());
    document.Accept(writer);
    fclose(fp);

    Carrot::JSON::clearReferences();

    updateGraph.resetChangeFlag();
    renderGraph.resetChangeFlag();
    reloadPreview();
}

void Tools::ParticleEditor::generateParticleFile(const std::filesystem::path& filename) {
    auto updateExpressions = updateGraph.generateExpressionsFromTerminalNodes();
    auto fragmentExpressions = renderGraph.generateExpressionsFromTerminalNodes();
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

void Tools::ParticleEditor::onFrame(Carrot::Render::Context renderContext) {
    if(&renderContext.pViewport == &previewViewport) {
        if(previewSystem) {
            previewSystem->onFrame(renderContext);

            engine.performSingleTimeGraphicsCommands([&](vk::CommandBuffer& cmds) {
                previewRenderGraph->execute(renderContext, cmds);
            });
        }

    } else {
        float menuBarHeight = 0;
        if(ImGui::BeginMainMenuBar()) {
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
                    auto expressions = renderGraph.generateExpressionsFromTerminalNodes();
                    for(const auto& expr : expressions) {
                        std::cout << expr->toString() << '\n';
                    }
                    std::cout << std::endl;
                }

                ImGui::EndMenu();
            }

            menuBarHeight = ImGui::GetWindowSize().y;
            ImGui::EndMainMenuBar();
        }

        auto& viewport = *ImGui::GetMainViewport();
        ImGui::SetNextWindowViewport(viewport.ID);
        ImGui::SetNextWindowPos(ImVec2(viewport.Pos.x, viewport.Pos.y+menuBarHeight));
        ImGui::SetNextWindowSize(ImVec2(engine.getVulkanDriver().getFinalRenderSize().width,
                                        engine.getVulkanDriver().getFinalRenderSize().height - menuBarHeight));
        if(ImGui::Begin("ParticleEditorWindow", nullptr, ImGuiWindowFlags_::ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_::ImGuiWindowFlags_NoBringToFrontOnFocus)) {
            if(ImGui::BeginTabBar("ParticleEditorTabs")) {
                if(ImGui::BeginTabItem("Update##tab particle editor")) {
                    updateUpdateGraph(renderContext);
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("Render##tab particle editor")) {
                    updateRenderGraph(renderContext);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();

        templateEditor.onFrame(renderContext);

        ProjectMenuHolder::onFrame(renderContext);

        UIParticlePreview(renderContext);
    }
}

void Tools::ParticleEditor::UIParticlePreview(Carrot::Render::Context renderContext) {
    ImVec2 previewSize { 200, 200 };
    ImGui::SetNextWindowPos(ImVec2(WINDOW_WIDTH/2 - previewSize.x/2, WINDOW_HEIGHT - previewSize.y/2), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(previewSize, ImGuiCond_FirstUseEver);
    if(ImGui::Begin("preview##preview", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground)) {
        auto windowSize = ImGui::GetContentRegionAvail();
        auto& texture = renderContext.renderer.getVulkanDriver().getTextureRepository().get(previewColorTexture, renderContext.swapchainIndex);
        ImGui::Image(texture.getImguiID(), windowSize);
        // TODO camera controls
    }
    ImGui::End();
}

void Tools::ParticleEditor::reloadPreview() {
    auto updateExpressions = updateGraph.generateExpressionsFromTerminalNodes();
    auto fragmentExpressions = renderGraph.generateExpressionsFromTerminalNodes();
    ParticleShaderGenerator updateGenerator(ParticleShaderMode::Compute, "ParticleEditor-Preview");
    ParticleShaderGenerator fragmentGenerator(ParticleShaderMode::Fragment, "ParticleEditor-Preview");

    auto computeShader = updateGenerator.compileToSPIRV(updateExpressions);
    auto fragmentShader = fragmentGenerator.compileToSPIRV(fragmentExpressions);
    bool isOpaque = false; // TODO: determine via render graph

    previewBlueprint = std::make_unique<Carrot::ParticleBlueprint>(std::move(computeShader), std::move(fragmentShader), isOpaque);
    previewSystem = std::make_unique<Carrot::ParticleSystem>(engine, *previewBlueprint, MaxPreviewParticles);

    auto& emitter = *previewSystem->createEmitter();
    emitter.setRate(10.0f);
}

void Tools::ParticleEditor::tick(double deltaTime) {
    updateGraph.tick(deltaTime);
    renderGraph.tick(deltaTime);

    if(previewSystem) {
        previewSystem->tick(deltaTime);
    }

    hasUnsavedChanges = updateGraph.hasChanges() || renderGraph.hasChanges();
}

Tools::ParticleEditor::~ParticleEditor() {
}

void Tools::ParticleEditor::performLoad(std::filesystem::path fileToOpen) {
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

void Tools::ParticleEditor::clear() {
    updateGraph.clear();
    renderGraph.clear();
    hasUnsavedChanges = false;
    reloadPreview();
}

bool Tools::ParticleEditor::showUnsavedChangesPopup() {
    return hasUnsavedChanges;
}

void Tools::ParticleEditor::onSwapchainImageCountChange(size_t newCount) {
    previewRenderGraph->onSwapchainImageCountChange(newCount);
}

void Tools::ParticleEditor::onSwapchainSizeChange(int newWidth, int newHeight) {
    previewRenderGraph->onSwapchainSizeChange(newWidth, newHeight);
}
