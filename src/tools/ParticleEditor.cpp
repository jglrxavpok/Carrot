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

#include "engine/io/IO.h"
#include "engine/utils/JSON.h"
#include "tools/generators/ParticleShaderGenerator.h"
#include "engine/render/particles/ParticleBlueprint.h"
#include "engine/render/RenderGraph.h"

#include <spirv_glsl.hpp>
#include <nfd.h>

namespace ed = ax::NodeEditor;

std::filesystem::path Tools::ParticleEditor::EmptyProject = "";

Tools::ParticleEditor::ParticleEditor(Carrot::Engine& engine)
: Tools::ProjectMenuHolder(), engine(engine), settings("particle_editor"), templateEditor(engine),
updateGraph(engine, "UpdateEditor"), renderGraph(engine, "RenderEditor"), previewRenderGraph()
{
    previewRenderGraphBuilder = std::make_unique<Carrot::Render::GraphBuilder>(engine.getVulkanDriver());
    struct TmpPass {
        Carrot::Render::FrameResource color;
    };

    auto& tmpPass = previewRenderGraphBuilder->addPass<TmpPass>("tmp-test",
    [this](Carrot::Render::GraphBuilder& builder, Carrot::Render::Pass<TmpPass>& pass, TmpPass& data) {
        vk::ClearValue clearColor = vk::ClearColorValue(std::array{1.0f,0.0f,0.0f,1.0f});
        data.color = builder.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                {},
                                                vk::AttachmentLoadOp::eClear,
                                                clearColor,
                                                vk::ImageLayout::eColorAttachmentOptimal);

    },
    [](const Carrot::Render::CompiledPass& pass, const Carrot::Render::Context& frame, const TmpPass& data, vk::CommandBuffer& cmds) {
        // TODO
    });

    auto& composer = engine.getMainComposer();
    composer.add(tmpPass.getData().color, 0.5, 1.0, 0.5, 1.0, 0.5);

    previewRenderGraph = previewRenderGraphBuilder->compile();

    settings.load();
    settings.save();

    if(!settings.recentProjects.empty()) {
        settings.currentProject = *settings.recentProjects.begin();
    }

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
        performLoad(settings.currentProject.value());
    }

    std::filesystem::path testPath = "resources/node_templates/vec2length.json";
    templateEditor.performLoad(testPath);

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

void Tools::ParticleEditor::updateUpdateGraph(size_t frameIndex) {
    updateGraph.onFrame(frameIndex);
}

void Tools::ParticleEditor::updateRenderGraph(size_t frameIndex) {
    renderGraph.onFrame(frameIndex);
}

void Tools::ParticleEditor::saveToFile(std::filesystem::path path) {
    FILE* fp = fopen(path.string().c_str(), "wb"); // non-Windows use "w"

    char writeBuffer[65536];
    rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

    rapidjson::Writer<rapidjson::FileWriteStream> writer(os);

    rapidjson::Document document;
    document.SetObject();

    document.AddMember("update_graph", updateGraph.toJSON(document), document.GetAllocator());
    document.AddMember("render_graph", renderGraph.toJSON(document), document.GetAllocator());
    document.Accept(writer);
    fclose(fp);

    Carrot::JSON::clearReferences();

    updateGraph.resetChangeFlag();
    renderGraph.resetChangeFlag();
}

void Tools::ParticleEditor::onFrame(size_t frameIndex) {
    float menuBarHeight = 0;
    if(ImGui::BeginMainMenuBar()) {
        if(ImGui::BeginMenu("Project")) {
            drawProjectMenu();

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

            if(ImGui::MenuItem("Test SPIR-V generation")) {
                auto updateExpressions = updateGraph.generateExpressionsFromTerminalNodes();
                auto fragmentExpressions = renderGraph.generateExpressionsFromTerminalNodes();
                ParticleShaderGenerator updateGenerator(ParticleShaderMode::Compute, "ParticleEditor"); // TODO: use project name
                ParticleShaderGenerator fragmentGenerator(ParticleShaderMode::Fragment, "ParticleEditor"); // TODO: use project name

                auto computeShader = updateGenerator.compileToSPIRV(updateExpressions);
                auto fragmentShader = fragmentGenerator.compileToSPIRV(fragmentExpressions);

                Carrot::ParticleBlueprint blueprint(std::move(computeShader), std::move(fragmentShader));
                // TODO: let user decide output file
                Carrot::IO::writeFile("test-particle.particle", [&](std::ostream& out) {
                   out << blueprint;
                });

                Carrot::ParticleBlueprint loadTest("test-particle.particle");

                auto& result = blueprint.getFragmentShader();
                Carrot::IO::writeFile("test-shader-fragment.spv", (void*)result.data(), result.size() * sizeof(uint32_t));

                /* can be useful for debugging:*/
                 std::system("spirv-dis test-shader-fragment.spv > test-disassembly-fragment.txt");

                spirv_cross::CompilerGLSL compiler(result);

                spirv_cross::CompilerGLSL::Options options;
                options.version = 450;
                options.vulkan_semantics = true;
                compiler.set_common_options(options);

                std::cout << compiler.compile() << std::endl;
            }

            ImGui::EndMenu();
        }

        menuBarHeight = ImGui::GetWindowSize().y;
        ImGui::EndMainMenuBar();
    }

    ImGui::SetNextWindowPos(ImVec2(0, menuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(engine.getVulkanDriver().getFinalRenderSize().width,
                                    engine.getVulkanDriver().getFinalRenderSize().height - menuBarHeight));
    if(ImGui::Begin("ParticleEditorWindow", nullptr, ImGuiWindowFlags_::ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_::ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        if(ImGui::BeginTabBar("ParticleEditorTabs")) {
            if(ImGui::BeginTabItem("Update##tab particle editor")) {
                updateUpdateGraph(frameIndex);
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Render##tab particle editor")) {
                updateRenderGraph(frameIndex);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();


    templateEditor.onFrame(frameIndex);

    ProjectMenuHolder::onFrame(frameIndex);

    engine.performSingleTimeGraphicsCommands([&](vk::CommandBuffer& cmds) {
        previewRenderGraph->execute(engine.newRenderContext(frameIndex), cmds);
    });
}

void Tools::ParticleEditor::tick(double deltaTime) {
    updateGraph.tick(deltaTime);
    renderGraph.tick(deltaTime);

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
}

void Tools::ParticleEditor::clear() {
    updateGraph.clear();
    renderGraph.clear();
    hasUnsavedChanges = false;
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
