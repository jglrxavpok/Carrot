//
// Created by jglrxavpok on 02/05/2021.
//

#include "EditorNode.h"

#include <utility>
#include <core/utils/ImGuiUtils.hpp>

#include "imgui_internal.h"
#include "EditorGraph.h"
#include "core/utils/JSON.h"

namespace ed = ax::NodeEditor;

Tools::EditorNode::EditorNode(EditorGraph& graph, std::string title, std::string internalName)
: graph(graph), id(graph.nextID()), title(std::move(title)), internalName(std::move(internalName)) {}

Tools::EditorNode::EditorNode(EditorGraph& graph, std::string title, std::string internalName, const rapidjson::Value& json)
        : graph(graph), id(Carrot::UUID::fromString(json["node_id"].GetString())), title(std::move(title)), internalName(std::move(internalName)) {
    setPosition(ImVec2(json["x"].GetFloat(), json["y"].GetFloat()));
    graph.reserveID(id);
}

bool Tools::EditorNode::draw() {
    handlePosition();

    bool modified = false;
    u32 nodeID = graph.getEditorID(id);
    const ImVec2 nodeSize = ed::GetNodeSize(nodeID);
    const ImVec2 nodePosition = ed::GetNodePosition(nodeID);

    ed::BeginNode(nodeID);

    ImGui::PushID(nodeID);

    ImGui::BeginVertical("node");

    // split channel to draw background behind "title bar". The draw is split to compute size of title bar after drawing it, while still drawing the background behind it
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->ChannelsSplit(2); // background + foreground

    // channel 1 = foreground, decided by node
    {
        drawList->ChannelsSetCurrent(1);
        modified |= renderHeaderWidgets();
        ImGui::Spring();
    }

    // draw header background on channel 0
    {
        drawList->ChannelsSetCurrent(0);
        const float headerHeight = ImGui::GetCursorPosY() - nodePosition.y;
        const auto borderWidth = ed::GetStyle().NodeBorderWidth;
        ImVec2 min = nodePosition;
        ImVec2 max = min;
        max.x += nodeSize.x;
        max.y += headerHeight;

        min.x += borderWidth;
        min.y += borderWidth;
        max.x -= borderWidth;
        max.y -= borderWidth;

        drawList->AddRectFilled(min, max, getHeaderColor(), ed::GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop);
    }

    drawList->ChannelsMerge();

    ImGui::BeginHorizontal("content");

    ImGui::BeginVertical("inputs");
    ed::PushStyleVar(ed::StyleVar_PivotAlignment, ImVec2(0, 0.5f));
    ed::PushStyleVar(ed::StyleVar_PivotSize, ImVec2(0, 0));

    modified |= renderInputPins();
    ImGui::EndVertical();

    ImGui::BeginVertical("center");
    modified |= renderCenter();
    ImGui::EndVertical();

    if(inputs.empty()) {
        ImGui::Spring(1);
    }
    ImGui::BeginVertical("outputs");
    ed::PushStyleVar(ed::StyleVar_PivotAlignment, ImVec2(1.0f, 0.5f));
    ed::PushStyleVar(ed::StyleVar_PivotSize, ImVec2(0, 0));

    modified |= renderOutputPins();
    ImGui::EndVertical();

    if(outputs.empty()) {
        ImGui::Spring(1);
    }

    ImGui::EndHorizontal();
    ImGui::EndVertical();

    ImGui::PopID();
    ed::EndNode();
    return modified;
}

void Tools::EditorNode::handlePosition() {
    u32 nodeID = graph.getEditorID(id);
    const ImVec2 nodeSize = ed::GetNodeSize(nodeID);

    if(updatePosition) {
        ed::SetNodePosition(nodeID, position);
    } else if (followingMouseUntilClick) {
        ImVec2 mousePos = ImGui::GetMousePos();
        if (ed::IsActive() && ImGui::IsMousePosValid(&mousePos)) {
            ImVec2 nodePos;
            nodePos.x = mousePos.x - nodeSize.x/2;
            nodePos.y = mousePos.y - ImGui::GetTextLineHeight()/2;
            ed::SetNodePosition(nodeID, nodePos);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                followingMouseUntilClick = false;
            }
        }
    }
    position = ed::GetNodePosition(nodeID);
    updatePosition = false;
}

bool Tools::EditorNode::renderCenter() {
    return false;
}

bool Tools::EditorNode::renderHeaderWidgets() {
    ImGui::Text("%s", title.c_str());
    return false;
}

void Tools::EditorNode::renderSinglePin(bool isOutput, const Pin& pin) {
    // style inspired by Blender
    const ed::Style& style = ed::GetStyle();
    const float lineHeight = ImGui::GetTextLineHeight();
    const float circleRadius = lineHeight/2;
    const float innerCircleRadius = lineHeight/2 - 1;
    ImDrawList* pDrawList = ImGui::GetWindowDrawList();

    bool isConnected = false;
    if (isOutput) {
        isConnected = graph.hasLinksStartingFrom(pin);
    } else {
        isConnected = graph.hasLinksLeadingTo(pin);
    }

    auto drawPinCircle = [&](ImVec2 pinCircleCursorPosition) {
        pinCircleCursorPosition.y += circleRadius;
        pDrawList->AddCircleFilled(pinCircleCursorPosition, circleRadius, ImColor(style.Colors[ed::StyleColor_NodeBorder]));

        ImColor innerColor = ImGuiUtils::getCarrotColor();
        if (!isConnected) {
            innerColor.Value.x *= 0.3f;
            innerColor.Value.y *= 0.3f;
            innerColor.Value.z *= 0.3f;
        }
        pDrawList->AddCircleFilled(pinCircleCursorPosition, innerCircleRadius, innerColor);
    };
    if (!isOutput) { // force pin to be on left border of node
        const float cursorX = position.x;
        ImGui::SetCursorPosX(cursorX);

        ImVec2 pinCirclePosition = ImGui::GetCursorPos();
        drawPinCircle(pinCirclePosition);
    } else { // otherwise, we force the pin to be on right border of node, but this requires to compute the size of the pin first
        const float pinWidth = ImGui::CalcTextSize(pin.name.c_str()).x /*text*/ + ImGui::GetStyle().ItemSpacing.x + lineHeight /*icon*/ + style.NodePadding.z;
        const float cursorX = position.x + ed::GetNodeSize(graph.getEditorID(id)).x - pinWidth;
        ImGui::SetCursorPosX(cursorX);

        ImVec2 pinCirclePosition = ImGui::GetCursorPos() + ImVec2(pinWidth, 0);
        drawPinCircle(pinCirclePosition);
    }

    ed::BeginPin(graph.getEditorID(pin.id), isOutput ? ed::PinKind::Output : ed::PinKind::Input);
    ImGui::BeginHorizontal(&pin.id);
    if (!isOutput) {
        ImGui::Dummy(ImVec2(circleRadius/2, circleRadius*2));
    }
    ImGui::TextUnformatted(pin.name.c_str());
    auto icon = graph.getImGuiTextures().getExpressionType(pin.getExpressionType());
    ImGui::Image(icon, ImVec2(lineHeight, lineHeight));
    ImGui::EndHorizontal();
    ed::EndPin();
}

bool Tools::EditorNode::renderInputPins() {
    for (auto& pin : inputs) {
        renderSinglePin(false, *pin);
    }
    return false;
}

bool Tools::EditorNode::renderOutputPins() {
    for (auto& pin : outputs) {
        renderSinglePin(true, *pin);
    }
    return false;
}

Tools::Input& Tools::EditorNode::newInput(std::string name, Carrot::ExpressionType type) {
    auto result = std::make_shared<Input>(*this, type, inputs.size(), graph.nextID(), name);
    inputs.push_back(result);
    graph.registerPin(result);
    return *result;
}

Tools::Output& Tools::EditorNode::newOutput(std::string name, Carrot::ExpressionType type) {
    auto result = std::make_shared<Output>(*this, type, outputs.size(), graph.nextID(), name);
    outputs.push_back(result);
    graph.registerPin(result);
    return *result;
}

Tools::EditorNode& Tools::EditorNode::setPosition(ImVec2 position) {
    this->position = position;
    updatePosition = true;
    return *this;
}

void Tools::EditorNode::followMouseUntilClick() {
    followingMouseUntilClick = true;
}


Tools::EditorNode::~EditorNode() {

}

ImColor Tools::EditorNode::getHeaderColor() const {
    return ImColor(0.3f, 0.3f, 0.3f, 0.3f);
}

rapidjson::Value Tools::EditorNode::toJSON(rapidjson::Document& doc) const {
    rapidjson::Value object(rapidjson::kObjectType);
    object.AddMember("x", position.x, doc.GetAllocator());
    object.AddMember("y", position.y, doc.GetAllocator());
    auto uuidStr = id.toString();
    object.AddMember("node_id", Carrot::JSON::makeRef(uuidStr), doc.GetAllocator());
    object.AddMember("node_type", rapidjson::StringRef(internalName.c_str()), doc.GetAllocator());

    auto serialised = serialiseToJSON(doc);
    if(!serialised.IsNull() && serialised.IsObject()) {
        object.AddMember("extra", serialised, doc.GetAllocator());
    }
    return object;
}

std::vector<std::shared_ptr<Carrot::Expression>> Tools::EditorNode::getExpressionsFromInput(std::unordered_set<Carrot::UUID>& activeLinks) const {
    std::vector<std::shared_ptr<Carrot::Expression>> expressions;
    size_t inputIndex = 0;
    for(const auto& input : inputs) {
        bool foundOne = false;
        for(const auto& link : graph.getLinksLeadingTo(*input)) {
            if(auto pinFrom = link.from.lock()) {
                auto expr = pinFrom->owner.toExpression(pinFrom->pinIndex, activeLinks);
                if (expr != nullptr) {
                    activeLinks.insert(link.id);
                    expressions.push_back(expr);
                    foundOne = true;
                }
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
    result.AddMember("node_id", Carrot::JSON::makeRef(owner.getID().toString()), document.GetAllocator());
    result.AddMember("pin_index", pinIndex, document.GetAllocator());
    result.AddMember("pin_type", rapidjson::StringRef(getType() == PinType::Input ? "input" : "output"), document.GetAllocator());
    return std::move(result);
}
