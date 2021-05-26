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

#include <spirv_glsl.hpp>

namespace ed = ax::NodeEditor;

Tools::ParticleEditor::ParticleEditor(Carrot::Engine& engine): engine(engine), templateGraph(engine, "TemplateEditor"), updateGraph(engine, "UpdateEditor"), renderGraph(engine, "RenderEditor") {
    {
        NodeLibraryMenuScope s1("Operators", &updateGraph);
        NodeLibraryMenuScope s2("Operators", &renderGraph);
        NodeLibraryMenuScope s3("Operators", &templateGraph);
        addCommonOperators(updateGraph);
        addCommonOperators(renderGraph);
        addCommonOperators(templateGraph);
    }
    {
        NodeLibraryMenuScope s1("Logic", &updateGraph);
        NodeLibraryMenuScope s2("Logic", &renderGraph);
        NodeLibraryMenuScope s3("Logic", &templateGraph);
        addCommonLogic(updateGraph);
        addCommonLogic(renderGraph);
        addCommonLogic(templateGraph);
    }

    {
        NodeLibraryMenuScope s1("Functions", &updateGraph);
        NodeLibraryMenuScope s2("Functions", &renderGraph);
        NodeLibraryMenuScope s3("Functions", &templateGraph);
        addCommonMath(updateGraph);
        addCommonMath(renderGraph);
        addCommonMath(templateGraph);
    }

    {
        NodeLibraryMenuScope s1("Inputs", &updateGraph);
        NodeLibraryMenuScope s2("Inputs", &renderGraph);
        NodeLibraryMenuScope s3("Inputs", &templateGraph);
        addCommonInputs(updateGraph);
        addCommonInputs(renderGraph);
        addCommonInputs(templateGraph);
    }

    {
        NodeLibraryMenuScope s("Named Inputs", &templateGraph);
        // TODO
    }
    {
        NodeLibraryMenuScope s("Named Outputs", &templateGraph);
        // TODO
    }

    {
        NodeLibraryMenuScope s1("Update Inputs", &updateGraph);
        NodeLibraryMenuScope s2("Render Inputs", &renderGraph);
        NodeLibraryMenuScope s3("Update/Render Inputs", &templateGraph);
        updateGraph.addVariableToLibrary<VariableNodeType::GetDeltaTime>();
        templateGraph.addVariableToLibrary<VariableNodeType::GetDeltaTime>();

        renderGraph.addVariableToLibrary<VariableNodeType::GetFragmentPosition>();
        templateGraph.addVariableToLibrary<VariableNodeType::GetFragmentPosition>();
    }
    {
        NodeLibraryMenuScope s1("Update Outputs", &updateGraph);
        updateGraph.addToLibrary<TerminalNodeType::SetVelocity>();
        updateGraph.addToLibrary<TerminalNodeType::SetSize>();

        NodeLibraryMenuScope s2("Render Outputs", &renderGraph);
        renderGraph.addToLibrary<TerminalNodeType::SetOutputColor>();
        renderGraph.addToLibrary<TerminalNodeType::DiscardPixel>();

        NodeLibraryMenuScope s3("Render/Update Outputs", &templateGraph);
        templateGraph.addToLibrary<TerminalNodeType::SetOutputColor>();
        templateGraph.addToLibrary<TerminalNodeType::DiscardPixel>();
        templateGraph.addToLibrary<TerminalNodeType::SetVelocity>();
        templateGraph.addToLibrary<TerminalNodeType::SetSize>();
    }

    if(std::filesystem::exists("particleGraphs.json")) {
        rapidjson::Document description;
        description.Parse(IO::readFileAsText("particleGraphs.json").c_str());

        updateGraph.loadFromJSON(description["update_graph"]);
        renderGraph.loadFromJSON(description["render_graph"]);
    }

    templateGraph.newNode<TemplateNode>("test");
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
    if(ImGui::Begin("Update Window", nullptr, ImGuiWindowFlags_NoDecoration)) {
        updateGraph.onFrame(frameIndex);
    }
    ImGui::End();
}

void Tools::ParticleEditor::updateRenderGraph(size_t frameIndex) {
    if(ImGui::Begin("Render Window", nullptr, ImGuiWindowFlags_NoDecoration)) {
        renderGraph.onFrame(frameIndex);
    }
    ImGui::End();
}

void Tools::ParticleEditor::onFrame(size_t frameIndex) {
    float menuBarHeight = 0;
    if(ImGui::BeginMainMenuBar()) {
        if(ImGui::BeginMenu("Project")) {
            if(ImGui::MenuItem("Load")) {
                rapidjson::Document description;
                description.Parse(IO::readFileAsText("particleGraphs.json").c_str());

                updateGraph.loadFromJSON(description["update_graph"]);
                renderGraph.loadFromJSON(description["render_graph"]);
            }

            if(ImGui::MenuItem("Save")) {
                struct Stream {
                    std::ofstream of {"particleGraphs.json"};
                    typedef char Ch;
                    void Put (Ch ch) {of.put (ch);}
                    void Flush() {}
                } stream;

                FILE* fp = fopen("particleGraphs.json", "wb"); // non-Windows use "w"

                char writeBuffer[65536];
                rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

                rapidjson::Writer<rapidjson::FileWriteStream> writer(os);

                rapidjson::Document document;
                document.SetObject();

                document.AddMember("update_graph", updateGraph.toJSON(document), document.GetAllocator());
                document.AddMember("render_graph", renderGraph.toJSON(document), document.GetAllocator());
                Carrot::JSON::clearReferences();
                document.Accept(writer);

                fclose(fp);
            }

            if(ImGui::MenuItem("Save as...")) {
                // TODO
            }

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Tests")) {
            if(ImGui::MenuItem("Print tree")) {
                auto expressions = renderGraph.generateExpressions();
                for(const auto& expr : expressions) {
                    std::cout << expr->toString() << '\n';
                }
                std::cout << std::endl;
            }

            if(ImGui::MenuItem("Test SPIR-V generation")) {
                auto expressions = renderGraph.generateExpressions();
                ParticleShaderGenerator generator(ParticleShaderMode::Fragment, "ParticleEditor"); // TODO: use project name

                auto result = generator.compileToSPIRV(expressions);
                IO::writeFile("test-shader-fragment.spv", (void*)result.data(), result.size() * sizeof(uint32_t));

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
    ImGui::SetNextWindowSize(ImVec2(engine.getVulkanDriver().getSwapchainExtent().width/2, engine.getVulkanDriver().getSwapchainExtent().height-menuBarHeight));

    updateUpdateGraph(frameIndex);

    ImGui::SetNextWindowPos(ImVec2(engine.getVulkanDriver().getSwapchainExtent().width/2,menuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(engine.getVulkanDriver().getSwapchainExtent().width/2, engine.getVulkanDriver().getSwapchainExtent().height-menuBarHeight));
    updateRenderGraph(frameIndex);

    bool editingTemplate = true; // TODO: move to member
    if(editingTemplate) {
        if(ImGui::Begin("Template editor")) {
            if(ImGui::BeginMenuBar()) {
                if(ImGui::BeginMenu("File##template")) {
                    if(ImGui::MenuItem("New##template")) {
                        // TODO
                    }
                    if(ImGui::MenuItem("Open##template")) {
                        // TODO
                    }
                    if(ImGui::MenuItem("Save##template")) {
                        // TODO
                    }
                    if(ImGui::MenuItem("Save as...##template")) {
                        // TODO
                    }

                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }
            templateGraph.onFrame(frameIndex);
        }
        ImGui::End();
    }
}

void Tools::ParticleEditor::tick(double deltaTime) {
    updateGraph.tick(deltaTime);
    renderGraph.tick(deltaTime);
}

Tools::ParticleEditor::~ParticleEditor() {
}
