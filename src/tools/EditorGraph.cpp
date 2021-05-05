//
// Created by jglrxavpok on 02/05/2021.
//

#include "EditorGraph.h"

#include <utility>
#include "nodes/Arithmetics.hpp"
#include "nodes/Constants.hpp"

namespace ed = ax::NodeEditor;

Tools::EditorGraph::EditorGraph(Carrot::Engine& engine, std::string name): engine(engine), name(std::move(name)) {
    config.NavigateButtonIndex = 2;
    g_Context = ed::CreateEditor(&config);

    ed::SetCurrentEditor(g_Context);
}

void Tools::EditorGraph::onFrame(size_t frameIndex) {
    ed::SetCurrentEditor(g_Context);

    ed::Begin(name.c_str());

    for(auto& [id, node] : id2node) {
        node->draw();
    }

    for(const auto& link : links) {
        if(auto input = link.inputPin.lock()) {
            if(auto output = link.outputPin.lock()) {
                ed::Link(link.id, input->id, output->id);
            }
        }
    }

    if(ed::BeginCreate()) {
        ed::PinId input;
        ed::PinId output;

        if(ed::QueryNewLink(&input, &output)) {
            if(input && output) {
                if(ed::AcceptNewItem()) {
                    auto& inputPin = id2pin[input.Get()];
                    auto& outputPin = id2pin[output.Get()];
                    if(inputPin && outputPin) {
                        if(canLink(*inputPin, *outputPin)) {
                            links.push_back(Link {
                                    .id = uniqueID++,
                                    .inputPin = inputPin,
                                    .outputPin = outputPin,
                            });
                        }
                    }
                }
            }
        }
    }
    ed::EndCreate();

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
}

void Tools::EditorGraph::removeNode(Tools::EditorNode& node) {
    id2node[node.getID().Get()] = nullptr;
}

void Tools::EditorGraph::registerPin(ed::PinId id, std::shared_ptr<Tools::Pin> pin) {
    id2pin[id.Get()] = std::move(pin);
}

void Tools::EditorGraph::unregisterPin(ed::PinId id) {
    id2pin[id.Get()] = nullptr;
}

bool Tools::EditorGraph::canLink(Tools::Pin& pinA, Tools::Pin& pinB) {
    if(pinA.getType() == pinB.getType()) {
        return false;
    }
    if(pinA.owner.getID() == pinB.owner.getID())
        return false;
    // TODO: check cycle
    return true;
}

Tools::EditorGraph::~EditorGraph() {
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

    for(const auto& [id, node] : id2node) {
        uniqueID = std::max(uniqueID, node->highestUsedID());
    }
    uniqueID++;

    auto getPin = [&](const rapidjson::Value& json) -> std::shared_ptr<Pin> {
        auto node = id2node[json["node_id"].GetInt64()];
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
                    .id = uniqueID++,
                    .inputPin = from,
                    .outputPin = to,
            });
        }
    }

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
        if(auto input = link.inputPin.lock()) {
            if (auto output = link.outputPin.lock()) {
                linkArray.PushBack(std::move(rapidjson::Value(rapidjson::kObjectType)
                                                     .AddMember("from", std::move(input->toJSONReference(document)),
                                                                document.GetAllocator())
                                                     .AddMember("to", std::move(output->toJSONReference(document)),
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
    uniqueID = 1;
}