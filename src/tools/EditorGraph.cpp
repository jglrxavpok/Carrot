//
// Created by jglrxavpok on 02/05/2021.
//

#include "EditorGraph.h"

#include <utility>
#include "nodes/Arithmetics.hpp"
#include "nodes/Constants.hpp"
#include <engine/utils/ImGuiUtils.hpp>

namespace ed = ax::NodeEditor;

Tools::EditorGraph::EditorGraph(Carrot::Engine& engine, std::string name): engine(engine), name(std::move(name)) {
    config.NavigateButtonIndex = 2;
    g_Context = ed::CreateEditor(&config);

    ed::SetCurrentEditor(g_Context);
}

void Tools::EditorGraph::onFrame(size_t frameIndex) {
    ed::SetCurrentEditor(g_Context);
    ed::EnableShortcuts(true);

    ed::Begin(name.c_str());

    for(auto& [id, node] : id2node) {
        node->draw();
    }

    for(const auto& link : links) {
        if(auto input = link.to.lock()) {
            if(auto output = link.from.lock()) {
                ed::Link(getEditorID(link.id), getEditorID(input->id), getEditorID(output->id));
            }
        }
    }

    if(ed::BeginCreate()) {
        ed::PinId input;
        ed::PinId output;

        if(ed::QueryNewLink(&input, &output)) {
            if(input && output) {
                auto& pinA = id2pin[id2uuid[input.Get()]];
                auto& pinB = id2pin[id2uuid[output.Get()]];
                if(pinA && pinB) {
                    handleLinkCreation(pinA, pinB);
                }
            }
        }
    }
    ed::EndCreate();

    if(ed::BeginDelete()) {
        ed::LinkId linkToDelete;
        while(ed::QueryDeletedLink(&linkToDelete)) {
            auto link = std::find_if(links.begin(), links.end(), [&](const auto& link) { return linkToDelete.Get() == uuid2id[link.id]; });
            if(link != links.end() && ed::AcceptDeletedItem()) {
                links.erase(link);
            }
        }

        ed::NodeId nodeToDelete;
        while(ed::QueryDeletedNode(&nodeToDelete)) {
            auto node = id2node.find(id2uuid[nodeToDelete.Get()]);
            if(node != id2node.end()) {
                id2node.erase(node);
            }
        }

        ed::EndDelete();
    }

    ed::End();

    if(ImGui::BeginPopupContextWindow("##popup")) {
        if (ImGui::BeginMenu("Add node")) {
            for(const auto& [internalName, nodeCreator] : nodeLibrary) {
                auto title = internalName2title[internalName];
                if(ImGui::MenuItem(title.c_str())) {
                    (*nodeCreator)(*this).setPosition(ed::ScreenToCanvas(ImGui::GetMousePos()));
                }
            }

            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    ed::EnableShortcuts(false);
}

void Tools::EditorGraph::handleLinkCreation(std::shared_ptr<Tools::Pin> pinA, std::shared_ptr<Tools::Pin> pinB) {
    if(pinA->getType() != pinB->getType()) {
        auto inputPin = pinA->getType() == PinType::Input ? pinA : pinB;
        auto outputPin = pinA->getType() != PinType::Input ? pinA : pinB;
        auto linkPossibility = canLink(*outputPin, *inputPin);

        if(linkPossibility == LinkPossibility::Possible) {
            if(ed::AcceptNewItem(ImColor(128, 255, 128), 4.0f)) {
                links.push_back(Link {
                        .id = nextID(),
                        .from = outputPin,
                        .to = inputPin,
                });
            }
        } else {
            ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
            switch (linkPossibility) {
                case LinkPossibility::TooManyInputs:
                    showLabel("Too many inputs!");
                    break;
                case LinkPossibility::CyclicalGraph:
                    showLabel("Cyclical graph!");
                    break;
                case LinkPossibility::CannotLinkToSelf:
                    showLabel("Cannot link to self!");
                    break;
            }
        }
    } else {
        showLabel("Cannot link same pin type together!");
        ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
    }
}

void Tools::EditorGraph::removeNode(Tools::EditorNode& node) {
    id2node[node.getID()] = nullptr;
    id2uuid.erase(uuid2id[node.getID()]);
    uuid2id.erase(node.getID());
}

void Tools::EditorGraph::registerPin(std::shared_ptr<Tools::Pin> pin) {
    id2pin[pin->id] = std::move(pin);
}

void Tools::EditorGraph::unregisterPin(shared_ptr<Pin> pin) {
    id2pin[pin->id] = nullptr;
    id2uuid.erase(uuid2id[pin->id]);
    uuid2id.erase(pin->id);
}

void Tools::EditorGraph::unregisterNode(const Tools::EditorNode& node) {
    id2pin[node.getID()] = nullptr;
    id2uuid.erase(uuid2id[node.getID()]);
    uuid2id.erase(node.getID());

    auto deletePins = [&](const auto& pins) {
        for (const auto& pin : pins) {
            unregisterPin(pin);
        }
    };
    deletePins(node.getInputs());
    deletePins(node.getOutputs());
}

Tools::LinkPossibility Tools::EditorGraph::canLink(Tools::Pin& from, Tools::Pin& to) {
    if(from.owner.getID() == to.owner.getID())
        return LinkPossibility::CannotLinkToSelf;
    for(const auto& link : links) {
        if(auto linkTo = link.to.lock()) {
            if(linkTo.get() == &to) {

                return LinkPossibility::TooManyInputs;
            }
        }
    }
    // TODO: check cycle
    return LinkPossibility::Possible;
}

Tools::EditorGraph::~EditorGraph() {
    clear();
    ed::DestroyEditor(g_Context);
    g_Context = nullptr;
}

void Tools::EditorGraph::loadFromJSON(const rapidjson::Value& json) {
    clear();
    auto nodeArray = json["nodes"].GetArray();
    for(const auto& nodeJSON : nodeArray) {
        std::string internalName = nodeJSON["node_type"].GetString();
        auto& initialiser = nodeLibrary[internalName];
        if(initialiser) {
            (*initialiser)(*this, nodeJSON);
        } else {
            throw ParseError("Unknown internalName: " + internalName);
        }
    }

    auto getPin = [&](const rapidjson::Value& json) -> std::shared_ptr<Pin> {
        auto node = id2node[Carrot::fromString(json["node_id"].GetString())];
        if(!node) {
            throw ParseError("Invalid node ID: " + std::to_string(json["node_id"].GetInt64()));
        }

        uint32_t pinIndex = json["pin_index"].GetInt64();

        PinType type = std::string(json["pin_type"].GetString()) == "input" ? PinType::Input : PinType::Output;
        switch(type) {
            case PinType::Input:
                return node->getInputs()[pinIndex];
            case PinType::Output:
                return node->getOutputs()[pinIndex];
        }
        return nullptr;
    };
    auto linkArray = json["links"].GetArray();
    for(const auto& linkJSON : linkArray) {
        auto from = getPin(linkJSON["from"]);
        auto to = getPin(linkJSON["to"]);
        if(from && to) {
            links.push_back(Link {
                    .id = nextID(),
                    .from = from,
                    .to = to,
            });
        }
    }
}

uint32_t Tools::EditorGraph::nextFreeEditorID() {
    bool alreadyUsed;
    uint32_t result;
    do {
        result = uniqueID++;
        alreadyUsed = id2uuid.find(result) != id2uuid.end();
    } while(alreadyUsed);
    return result;
}

rapidjson::Value Tools::EditorGraph::toJSON(rapidjson::Document& document) {
    rapidjson::Value result;
    result.SetObject();

    auto& nodeArray = rapidjson::Value().SetArray();
    for(const auto& [id, node] : id2node) {
        nodeArray.PushBack(node->toJSON(document), document.GetAllocator());
    }
    result.AddMember("nodes", nodeArray, document.GetAllocator());

    auto& linkArray = rapidjson::Value().SetArray();
    for(const auto& link : links) {
        if(auto to = link.to.lock()) {
            if (auto from = link.from.lock()) {
                linkArray.PushBack(std::move(rapidjson::Value(rapidjson::kObjectType)
                                                     .AddMember("from", std::move(from->toJSONReference(document)),
                                                                document.GetAllocator())
                                                     .AddMember("to", std::move(to->toJSONReference(document)),
                                                                document.GetAllocator())
                ), document.GetAllocator());
            }
        }
    }
    result.AddMember("links", linkArray, document.GetAllocator());

    return result;
}

void Tools::EditorGraph::clear() {
    id2node.clear();
    id2pin.clear();
    id2uuid.clear();
    uuid2id.clear();
    uniqueID = 1;
}

Carrot::UUID Tools::EditorGraph::nextID() {
    Carrot::UUID uuid = Carrot::randomUUID();
    while(uuid2id.find(uuid) != uuid2id.end()) {
        uuid = Carrot::randomUUID();
    }
    reserveID(uuid);
    return uuid;
}

uint32_t Tools::EditorGraph::reserveID(const Carrot::UUID& uuid) {
    uint32_t id = nextFreeEditorID();
    id2uuid[id] = uuid;
    uuid2id[uuid] = id;
    return id;
}

uint32_t Tools::EditorGraph::getEditorID(const Carrot::UUID& uuid) {
    return uuid2id[uuid];
}

void Tools::EditorGraph::showLabel(const std::string& text, ImColor color) {
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeight());
    auto size = ImGui::CalcTextSize(text.c_str());

    auto padding = ImGui::GetStyle().FramePadding;
    auto spacing = ImGui::GetStyle().ItemSpacing;

    ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(spacing.x, -spacing.y));

    auto rectMin = ImGui::GetCursorScreenPos() - padding;
    auto rectMax = ImGui::GetCursorScreenPos() + size + padding;

    auto drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(rectMin, rectMax, color, size.y * 0.15f);
    ImGui::TextUnformatted(text.c_str());
}
