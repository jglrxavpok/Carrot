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
#include <nfd.h>

namespace ed = ax::NodeEditor;

std::filesystem::path Tools::ParticleEditor::EmptyProject = "";

Tools::ParticleEditor::ParticleEditor(Carrot::Engine& engine): engine(engine), settings("particle_editor"), templateGraph(engine, "TemplateEditor"), updateGraph(engine, "UpdateEditor"), renderGraph(engine, "RenderEditor") {
    settings.load();
    settings.save();

    if(!settings.recentProjects.empty()) {
        settings.currentProject = *settings.recentProjects.begin();
    }

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

    templateGraph.newNode<TemplateNode>("test");

    if(settings.currentProject) {
        fileToOpen = settings.currentProject.value();
        performLoad();
    }
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
    if(ImGui::Begin("Update Window", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        updateGraph.onFrame(frameIndex);
    }
    ImGui::End();
}

void Tools::ParticleEditor::updateRenderGraph(size_t frameIndex) {
    if(ImGui::Begin("Render Window", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        renderGraph.onFrame(frameIndex);
    }
    ImGui::End();
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

bool Tools::ParticleEditor::triggerSaveAs(std::filesystem::path& path) {
    nfdchar_t* savePath;

    // prepare filters for the dialog
    nfdfilteritem_t filterItem[1] = {{"Particle Project", "json"}};

    // show the dialog
    nfdresult_t result = NFD_SaveDialog(&savePath, filterItem, 1, nullptr, "UntitledParticle.json");
    if (result == NFD_OKAY) {
        saveToFile(savePath);
        path = savePath;
        // remember to free the memory (since NFD_OKAY is returned)
        NFD_FreePath(savePath);
        return true;
    } else if (result == NFD_CANCEL) {
        return false;
    } else {
        std::string msg = "Error: ";
        msg += NFD_GetError();
        throw std::runtime_error(msg);
    }
}

bool Tools::ParticleEditor::triggerSave() {
    if(settings.currentProject) {
        saveToFile(settings.currentProject.value());
        return true;
    } else {
        std::optional<std::filesystem::path> prevProject = settings.currentProject;
        settings.currentProject = "";
        bool result = triggerSaveAs(settings.currentProject.value());
        if(!result) {
            settings.currentProject = prevProject;
        }
        return result;
    }
}

void Tools::ParticleEditor::onFrame(size_t frameIndex) {
    float menuBarHeight = 0;
    if(ImGui::BeginMainMenuBar()) {
        if(ImGui::BeginMenu("Project")) {
            if(ImGui::MenuItem("New##particleEditor")) {
                scheduleLoad(EmptyProject);
            }
            if(ImGui::MenuItem("Open##particleEditor")) {
                nfdchar_t* outPath;

                // prepare filters for the dialog
                nfdfilteritem_t filterItem[1] = {{"Particle Project", "json"}};

                // show the dialog
                nfdresult_t result = NFD_OpenDialog(&outPath, filterItem, 1, nullptr);
                if (result == NFD_OKAY) {
                    scheduleLoad(outPath);
                    // remember to free the memory (since NFD_OKAY is returned)
                    NFD_FreePath(outPath);
                } else if (result == NFD_CANCEL) {
                    // no-op
                } else {
                    std::string msg = "Error: ";
                    msg += NFD_GetError();
                    throw std::runtime_error(msg);
                }
            }
            if(ImGui::BeginMenu("Open Recent##particleEditor", !settings.recentProjects.empty())) {
                auto recentProjectsCopy = settings.recentProjects;
                for(const auto& project : recentProjectsCopy) {
                    if(ImGui::MenuItem(project.string().c_str())) {
                        scheduleLoad(project);
                    }
                }

                ImGui::EndMenu();
            }

            if(ImGui::MenuItem("Save##particleEditor")) {
                if(triggerSave()) {
                    settings.addToRecentProjects(settings.currentProject.value());
                }
            }

            if(ImGui::MenuItem("Save as...##particleEditor")) {
                std::optional<std::filesystem::path> prevProject = settings.currentProject;
                settings.currentProject = "";
                bool result = triggerSaveAs(settings.currentProject.value());
                if(!result) {
                    settings.currentProject = prevProject;
                } else {
                    settings.addToRecentProjects(settings.currentProject.value());
                }
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
        if(ImGui::Begin("Template editor", nullptr, ImGuiWindowFlags_MenuBar)) {
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
                ImGui::EndMenuBar();
            }

            ImGui::Text("hiii");

            templateGraph.onFrame(frameIndex);
        }
        ImGui::End();
    }

    if(tryingToOpenFile) {
        ImGui::OpenPopup("Unsaved changes");
    }
    if(ImGui::BeginPopupModal("Unsaved changes")) {
        ImGui::Text("You currently have unsaved changes!");
        ImGui::Text("Do you still want to continue?");

        if(ImGui::Button("Save")) {
            if(triggerSave()) {
                performLoad();
            }
            tryingToOpenFile = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Don't save")) {
            performLoad();
            tryingToOpenFile = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel")) {
            fileToOpen = "";
            tryingToOpenFile = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Tools::ParticleEditor::tick(double deltaTime) {
    updateGraph.tick(deltaTime);
    renderGraph.tick(deltaTime);

    hasUnsavedChanges = updateGraph.hasChanges() || renderGraph.hasChanges();
}

Tools::ParticleEditor::~ParticleEditor() {
}

void Tools::ParticleEditor::performLoad() {
    if(fileToOpen == EmptyProject) {
        updateGraph.clear();
        renderGraph.clear();
        hasUnsavedChanges = false;
        settings.currentProject.reset();
        return;
    }
    rapidjson::Document description;
    description.Parse(IO::readFileAsText(fileToOpen.string()).c_str());

    updateGraph.loadFromJSON(description["update_graph"]);
    renderGraph.loadFromJSON(description["render_graph"]);
    settings.currentProject = fileToOpen;

    settings.addToRecentProjects(fileToOpen);
    hasUnsavedChanges = false;
}

void Tools::ParticleEditor::scheduleLoad(std::filesystem::path path) {
    fileToOpen = path;
    if(hasUnsavedChanges) {
        ImGui::OpenPopup("Unsaved changes");
        tryingToOpenFile = true;
    } else {
        performLoad();
    }
}

void Tools::ParticleEditor::clear() {
    updateGraph.clear();
    renderGraph.clear();
    hasUnsavedChanges = false;
}
