//
// Created by jglrxavpok on 01/05/2021.
//

#include "ParticleEditor.h"
#include "nodes/Arithmetics.hpp"
#include "nodes/Constants.hpp"
#include <iostream>
#include <fstream>
#include <rapidjson/writer.h>
#include <rapidjson/filewritestream.h>

#include "engine/io/IO.h"
#include "engine/utils/JSON.h"

namespace ed = ax::NodeEditor;

Tools::ParticleEditor::ParticleEditor(Carrot::Engine& engine): engine(engine), updateGraph(engine, "UpdateEditor"), renderGraph(engine, "RenderEditor") {
    addCommonNodes(updateGraph);
    addCommonNodes(renderGraph);

    updateGraph.addToLibrary<TerminalNodeType::SetVelocity>();
}

void Tools::ParticleEditor::addCommonNodes(Tools::EditorGraph& graph) {
    graph.addToLibrary<FloatConstantNode>("float_constant", "Constant");
    graph.addToLibrary<AddNode>("add", "Add");
    graph.addToLibrary<SubNode>("sub", "Subtract");
    graph.addToLibrary<MultNode>("mult", "Multiply");
    graph.addToLibrary<DivNode>("div", "Divide");
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
                description.Parse(IO::readFileAsText("updateGraph.json").c_str());

                updateGraph.loadFromJSON(description["update_graph"]);
            }

            if(ImGui::MenuItem("Save")) {
                struct Stream {
                    std::ofstream of {"updateGraph.json"};
                    typedef char Ch;
                    void Put (Ch ch) {of.put (ch);}
                    void Flush() {}
                } stream;

                FILE* fp = fopen("updateGraph.json", "wb"); // non-Windows use "w"

                char writeBuffer[65536];
                rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

                rapidjson::Writer<rapidjson::FileWriteStream> writer(os);

                rapidjson::Document document;
                document.SetObject();

                document.AddMember("update_graph", updateGraph.toJSON(document), document.GetAllocator());
                Carrot::JSON::clearReferences();
                document.Accept(writer);

                fclose(fp);
            }

            if(ImGui::MenuItem("Save as...")) {
                // TODO
            }

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Placeholder")) {
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
}

void Tools::ParticleEditor::tick(double deltaTime) {
    updateGraph.tick(deltaTime);
    renderGraph.tick(deltaTime);
}

Tools::ParticleEditor::~ParticleEditor() {
}
