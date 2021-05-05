//
// Created by jglrxavpok on 02/05/2021.
//

#include "EditorNode.h"

#include <utility>
#include "imgui_internal.h"
#include "EditorGraph.h"

namespace ed = ax::NodeEditor;

Tools::EditorNode::EditorNode(EditorGraph& graph, std::string title, std::string internalName)
: graph(graph), id(graph.nextID()), title(std::move(title)), internalName(std::move(internalName)) {}

Tools::EditorNode::EditorNode(EditorGraph& graph, std::string title, std::string internalName, const rapidjson::Value& json)
        : graph(graph), id(json["node_id"].GetInt64()), title(std::move(title)), internalName(std::move(internalName)) {
    setPosition(ImVec2(json["x"].GetFloat(), json["y"].GetFloat()));
}

void Tools::EditorNode::draw() {
    ed::BeginNode(id);

    ImGui::PushID(id.Get());

    ImGui::BeginVertical("node");
    ImGui::Spring();
    ImGui::Text("%s", title.c_str());
    ImGui::Spring();

    ImGui::BeginHorizontal("content");

    ImGui::BeginVertical("inputs");
    ImGui::Spring(0, 15 * 2);
    for (auto& pin : inputs) {
        ed::BeginPin(pin->id, ed::PinKind::Input);
        ImGui::BeginHorizontal(pin->id.AsPointer());
        ImGui::Text("> %s", pin->name.c_str());

        ImGui::EndHorizontal();
        ed::EndPin();
    }
    ImGui::EndVertical();

    ImGui::BeginVertical("center");
    renderCenter();
    ImGui::EndVertical();

    ImGui::BeginVertical("outputs");
    ImGui::Spring(0, 15 * 2);
    for (auto& pin : outputs) {
        ed::BeginPin(pin->id, ed::PinKind::Output);
        ImGui::BeginHorizontal(pin->id.AsPointer());
        ImGui::Text("%s >", pin->name.c_str());

        ImGui::EndHorizontal();
        ed::EndPin();
    }
    ImGui::EndVertical();


    ImGui::EndHorizontal();
    ImGui::EndVertical();

    ImGui::PopID();
    ed::EndNode();

    if(updatePosition) {
        ed::SetNodePosition(id, position);
    }
    position = ed::GetNodePosition(id);

    updatePosition = false;
}

Tools::Input& Tools::EditorNode::newInput(std::string name) {
    auto result = make_shared<Input>(*this, inputs.size(), graph.nextID(), name);
    inputs.push_back(result);
    graph.registerPin(result->id, result);
    return *result;
}

Tools::Output& Tools::EditorNode::newOutput(std::string name) {
    auto result = make_shared<Output>(*this, outputs.size(), graph.nextID(), name);
    outputs.push_back(result);
    graph.registerPin(result->id, result);
    return *result;
}

Tools::EditorNode& Tools::EditorNode::setPosition(ImVec2 position) {
    this->position = position;
    updatePosition = true;
    return *this;
}

Tools::EditorNode::~EditorNode() {
    for(const auto& pin : inputs) {
        graph.unregisterPin(pin->id);
    }
    for(const auto& pin : outputs) {
        graph.unregisterPin(pin->id);
    }
}

rapidjson::Value Tools::EditorNode::toJSON(rapidjson::Document& doc) {
    rapidjson::Value object(rapidjson::kObjectType);
    object.AddMember("x", position.x, doc.GetAllocator());
    object.AddMember("y", position.y, doc.GetAllocator());
    object.AddMember("node_id", id.Get(), doc.GetAllocator());
    object.AddMember("node_type", rapidjson::StringRef(internalName.c_str()), doc.GetAllocator());

    auto serialised = serialiseToJSON(doc);
    if(!serialised.IsNull() && serialised.IsObject()) {
        object.AddMember("extra", serialised, doc.GetAllocator());
    }
    return object;
}

const std::uint32_t Tools::EditorNode::highestUsedID() const {
    auto result = id.Get();
    for(const auto& i : inputs) {
        result = std::max(result, i->id.Get());
    }
    for(const auto& o : outputs) {
        result = std::max(result, o->id.Get());
    }
    return result;
}

rapidjson::Value Tools::Pin::toJSONReference(rapidjson::Document& document) {
    auto result = rapidjson::Value(rapidjson::kObjectType);
    result.AddMember("node_id", owner.getID().Get(), document.GetAllocator());
    result.AddMember("pin_index", pinIndex, document.GetAllocator());
    result.AddMember("pin_type", rapidjson::StringRef(getType() == PinType::Input ? "input" : "output"), document.GetAllocator());
    return std::move(result);
}
