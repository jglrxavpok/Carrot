//
// Created by jglrxavpok on 02/05/2021.
//

#include "EditorNode.h"

#include <utility>
#include "imgui_internal.h"
#include "EditorGraph.h"
#include "engine/utils/JSON.h"

namespace ed = ax::NodeEditor;

Tools::EditorNode::EditorNode(EditorGraph& graph, std::string title, std::string internalName)
: graph(graph), id(graph.nextID()), title(std::move(title)), internalName(std::move(internalName)) {}

Tools::EditorNode::EditorNode(EditorGraph& graph, std::string title, std::string internalName, const rapidjson::Value& json)
        : graph(graph), id(Carrot::fromString(json["node_id"].GetString())), title(std::move(title)), internalName(std::move(internalName)) {
    setPosition(ImVec2(json["x"].GetFloat(), json["y"].GetFloat()));
    graph.reserveID(id);
}

void Tools::EditorNode::draw() {
    ed::BeginNode(graph.getEditorID(id));

    ImGui::PushID(graph.getEditorID(id));

    ImGui::BeginVertical("node");
    ImGui::Spring();

    ImGui::Text("%s", title.c_str());

    ImGui::Spring();

    ImGui::BeginHorizontal("content");

    ImGui::BeginVertical("inputs");
    ed::PushStyleVar(ed::StyleVar_PivotAlignment, ImVec2(0, 0.5f));
    ed::PushStyleVar(ed::StyleVar_PivotSize, ImVec2(0, 0));

    for (auto& pin : inputs) {
        ed::BeginPin(graph.getEditorID(pin->id), ed::PinKind::Input);
        ImGui::BeginHorizontal(&pin->id);
        ImGui::Text("> %s (%s)", pin->name.c_str(), pin->getExpressionType().name().c_str());

        ImGui::EndHorizontal();
        ed::EndPin();
    }
    ImGui::EndVertical();

    ImGui::BeginVertical("center");
    renderCenter();
    ImGui::EndVertical();

    if(inputs.empty()) {
        ImGui::Spring(1);
    }
    ImGui::BeginVertical("outputs");
    ed::PushStyleVar(ed::StyleVar_PivotAlignment, ImVec2(1.0f, 0.5f));
    ed::PushStyleVar(ed::StyleVar_PivotSize, ImVec2(0, 0));

    for (auto& pin : outputs) {
        ed::BeginPin(graph.getEditorID(pin->id), ed::PinKind::Output);
        ImGui::BeginHorizontal(&pin->id);
        ImGui::Text("%s (%s) >", pin->name.c_str(), pin->getExpressionType().name().c_str());

        ImGui::EndHorizontal();
        ed::EndPin();
    }
    ImGui::EndVertical();

    if(outputs.empty()) {
        ImGui::Spring(1);
    }

    ImGui::EndHorizontal();
    ImGui::EndVertical();

    ImGui::PopID();
    ed::EndNode();

    if(updatePosition) {
        ed::SetNodePosition(graph.getEditorID(id), position);
    }
    position = ed::GetNodePosition(graph.getEditorID(id));

    updatePosition = false;
}

Tools::Input& Tools::EditorNode::newInput(std::string name, Carrot::ExpressionType type) {
    auto result = make_shared<Input>(*this, type, inputs.size(), graph.nextID(), name);
    inputs.push_back(result);
    graph.registerPin(result);
    return *result;
}

Tools::Output& Tools::EditorNode::newOutput(std::string name, Carrot::ExpressionType type) {
    auto result = make_shared<Output>(*this, type, outputs.size(), graph.nextID(), name);
    outputs.push_back(result);
    graph.registerPin(result);
    return *result;
}

Tools::EditorNode& Tools::EditorNode::setPosition(ImVec2 position) {
    this->position = position;
    updatePosition = true;
    return *this;
}

Tools::EditorNode::~EditorNode() {

}

rapidjson::Value Tools::EditorNode::toJSON(rapidjson::Document& doc) const {
    rapidjson::Value object(rapidjson::kObjectType);
    object.AddMember("x", position.x, doc.GetAllocator());
    object.AddMember("y", position.y, doc.GetAllocator());
    auto uuidStr = Carrot::toString(id);
    object.AddMember("node_id", Carrot::JSON::makeRef(uuidStr), doc.GetAllocator());
    object.AddMember("node_type", rapidjson::StringRef(internalName.c_str()), doc.GetAllocator());

    auto serialised = serialiseToJSON(doc);
    if(!serialised.IsNull() && serialised.IsObject()) {
        object.AddMember("extra", serialised, doc.GetAllocator());
    }
    return object;
}

std::vector<std::shared_ptr<Carrot::Expression>> Tools::EditorNode::getExpressionsFromInput() const {
    std::vector<std::shared_ptr<Carrot::Expression>> expressions;
    size_t inputIndex = 0;
    for(const auto& input : inputs) {
        bool foundOne = false;
        for(const auto& link : graph.getLinksLeadingTo(*input)) {
            if(auto pinFrom = link.from.lock()) {
                expressions.push_back(pinFrom->owner.toExpression(pinFrom->pinIndex));
                foundOne = true;
            }
        }

        if(!foundOne) {
            expressions.push_back(nullptr);
        }
        inputIndex++;

        // no more than one link per input
        assert(expressions.size() == inputIndex);
    }

    // no more than one link per input
    assert(expressions.size() == inputs.size());
    return std::move(expressions);
}

rapidjson::Value Tools::Pin::toJSONReference(rapidjson::Document& document) const {
    auto result = rapidjson::Value(rapidjson::kObjectType);
    result.AddMember("node_id", Carrot::JSON::makeRef(Carrot::toString(owner.getID())), document.GetAllocator());
    result.AddMember("pin_index", pinIndex, document.GetAllocator());
    result.AddMember("pin_type", rapidjson::StringRef(getType() == PinType::Input ? "input" : "output"), document.GetAllocator());
    return std::move(result);
}
