//
// Created by jglrxavpok on 28/05/2021.
//

#include "TemplateEditor.h"
#include <engine/io/IO.h>
#include <engine/utils/JSON.h>
#include <rapidjson/writer.h>
#include <rapidjson/filewritestream.h>
#include "nodes/NamedIO.h"

Tools::TemplateEditor::TemplateEditor(Carrot::Engine& engine): Tools::ProjectMenuHolder(), engine(engine), settings("node_templates"), graph(engine, "Template Editor") {
    settings.load();
    settings.save();
    std::fill_n(titleImGuiBuffer, '\0', sizeof(titleImGuiBuffer));
    attachSettings(settings);

    {
        NodeLibraryMenuScope s("Named I/O", &graph);
        graph.addToLibrary<NamedInputNode>("named_input", "Named Input");
        graph.addToLibrary<NamedOutputNode>("named_output", "Named Output");
    }
}

void Tools::TemplateEditor::tick(double deltaTime) {
    graph.tick(deltaTime);
}

void Tools::TemplateEditor::onFrame(size_t frameIndex) {
    if(!isOpen)
        return;
    if(ImGui::Begin("Template Editor", &isOpen, ImGuiWindowFlags_::ImGuiWindowFlags_MenuBar)) {
        if(ImGui::BeginMenuBar()) {
            if(ImGui::BeginMenu("File##template")) {
                drawProjectMenu();

                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if(ImGui::InputTextWithHint("Template title", "Title", titleImGuiBuffer, sizeof(titleImGuiBuffer))) {
            title = titleImGuiBuffer;
            hasUnsavedChanges = true;
        }

        graph.onFrame(frameIndex);
    }
    ImGui::End();

    ProjectMenuHolder::onFrame(frameIndex);

    hasUnsavedChanges = graph.hasChanges();
}

void Tools::TemplateEditor::open() {
    isOpen = true;
}

void Tools::TemplateEditor::performLoad(filesystem::path fileToOpen) {
    if(fileToOpen == EmptyProject) {
        graph.clear();
        hasUnsavedChanges = false;
        settings.currentProject.reset();
        return;
    }
    rapidjson::Document description;
    description.Parse(IO::readFileAsText(fileToOpen.string()).c_str());

    title = description["title"].GetString();
    strcpy_s(titleImGuiBuffer, title.c_str());

    if(description.HasMember("graph")) {
        graph.loadFromJSON(description["graph"]);
    } else {
        graph.clear();
    }

    settings.currentProject = fileToOpen;

    settings.addToRecentProjects(fileToOpen);
    hasUnsavedChanges = false;
}

void Tools::TemplateEditor::saveToFile(std::filesystem::path path) {
    FILE* fp = fopen(path.string().c_str(), "wb"); // non-Windows use "w"

    char writeBuffer[65536];
    rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

    rapidjson::Writer<rapidjson::FileWriteStream> writer(os);

    rapidjson::Document document;
    document.SetObject();

    document.AddMember("title", Carrot::JSON::makeRef(title), document.GetAllocator());
    document.AddMember("graph", graph.toJSON(document), document.GetAllocator());
    document.Accept(writer);
    fclose(fp);

    Carrot::JSON::clearReferences();

    graph.resetChangeFlag();
}

bool Tools::TemplateEditor::showUnsavedChangesPopup() {
    return hasUnsavedChanges;
}
