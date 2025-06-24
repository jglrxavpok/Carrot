//
// Created by jglrxavpok on 28/05/2021.
//

#include "TemplateEditor.h"
#include <rapidjson/writer.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include "../../../asset_tools/fertilizer/node_based/nodes/NamedIO.h"

Peeler::TemplateEditor::TemplateEditor(Carrot::Engine& engine): Tools::ProjectMenuHolder(), engine(engine), settings("node_templates"), graph("Template Editor") {
    settings.load();
    settings.save();
    std::fill_n(titleImGuiBuffer, '\0', sizeof(titleImGuiBuffer));
    attachSettings(settings);

    {
        Fertilizer::NodeLibraryMenuScope s("Named I/O", &graph);
        graph.addToLibrary<Fertilizer::NamedInputNode>("named_input", "Named Input");
        graph.addToLibrary<Fertilizer::NamedOutputNode>("named_output", "Named Output");
    }
}

void Peeler::TemplateEditor::tick(double deltaTime) {
    graph.tick(deltaTime);
}

void Peeler::TemplateEditor::onFrame(const Carrot::Render::Context& renderContext) {
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
        ImGui::SameLine();
        if(ImGui::InputTextWithHint("Template location", "Location/in/menu", menuLocationImGuiBuffer, sizeof(menuLocationImGuiBuffer))) {
            menuLocation = menuLocationImGuiBuffer;
            hasUnsavedChanges = true;
        }

        graph.draw();
    }
    ImGui::End();

    ProjectMenuHolder::onFrame(renderContext);

    hasUnsavedChanges = graph.hasChanges();
}

void Peeler::TemplateEditor::open() {
    isOpen = true;
}

void Peeler::TemplateEditor::performLoad(std::filesystem::path fileToOpen) {
    if(fileToOpen == EmptyProject) {
        graph.clear();
        hasUnsavedChanges = false;
        settings.currentProject.reset();
        return;
    }
    rapidjson::Document description;
    description.Parse(Carrot::IO::readFileAsText(fileToOpen.string()).c_str());

    title = description["title"].GetString();
    menuLocation = description["menu_location"].GetString();
    Carrot::strcpy(titleImGuiBuffer, title.c_str());
    Carrot::strcpy(menuLocationImGuiBuffer, menuLocation.c_str());

    if(description.HasMember("graph")) {
        graph.loadFromJSON(description["graph"]);
    } else {
        graph.clear();
    }

    settings.currentProject = fileToOpen;

    settings.addToRecentProjects(fileToOpen);
    hasUnsavedChanges = false;
}

void Peeler::TemplateEditor::saveToFile(std::filesystem::path path) {
    FILE* fp = fopen(path.string().c_str(), "wb"); // non-Windows use "w"

    char writeBuffer[65536];
    rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

    rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);

    rapidjson::Document document;
    document.SetObject();

    document.AddMember("title", Carrot::JSON::makeRef(title), document.GetAllocator());
    document.AddMember("graph", graph.toJSON(document), document.GetAllocator());
    document.AddMember("menu_location", Carrot::JSON::makeRef(menuLocation), document.GetAllocator());

    rapidjson::Value inputs;
    inputs.SetArray();

    rapidjson::Value outputs;
    outputs.SetArray();

    for(const auto& [id, node] : graph.getNodes()) {
        if(auto namedInput = std::dynamic_pointer_cast<Fertilizer::NamedInputNode>(node)) {
            inputs.PushBack(rapidjson::Value(rapidjson::kObjectType)
                                    .AddMember("name", Carrot::JSON::makeRef(namedInput->getIOName()), document.GetAllocator())
                                    .AddMember("type", Carrot::JSON::makeRef(namedInput->getType().name()), document.GetAllocator())
                                    .AddMember("dimensions", namedInput->getDimensionCount(), document.GetAllocator())
                    , document.GetAllocator());
        }
        if(auto namedOutput = std::dynamic_pointer_cast<Fertilizer::NamedOutputNode>(node)) {
            outputs.PushBack(rapidjson::Value(rapidjson::kObjectType)
                                     .AddMember("name", Carrot::JSON::makeRef(namedOutput->getIOName()), document.GetAllocator())
                                     .AddMember("type", Carrot::JSON::makeRef(namedOutput->getType().name()), document.GetAllocator())
                                     .AddMember("dimensions", namedOutput->getDimensionCount(), document.GetAllocator())
                    , document.GetAllocator());
        }
    }
    document.AddMember("inputs", inputs, document.GetAllocator());
    document.AddMember("outputs", outputs, document.GetAllocator());

    document.Accept(writer);
    fclose(fp);

    Carrot::JSON::clearReferences();

    graph.resetChangeFlag();
}

bool Peeler::TemplateEditor::showUnsavedChangesPopup() {
    return hasUnsavedChanges;
}

void Peeler::TemplateEditor::onCutShortcut(const Carrot::Render::Context& frame) {
    graph.onCutShortcut();
}

void Peeler::TemplateEditor::onCopyShortcut(const Carrot::Render::Context& frame) {
    graph.onCopyShortcut();
}

void Peeler::TemplateEditor::onPasteShortcut(const Carrot::Render::Context& frame) {
    graph.onPasteShortcut();
}

void Peeler::TemplateEditor::onDuplicateShortcut(const Carrot::Render::Context& frame) {
    graph.onDuplicateShortcut();
}
