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

Fertilizer::EditorNode::EditorNode(EditorGraph& graph, std::string title, std::string internalName)
: graph(graph), id(graph.nextID()), title(std::move(title)), internalName(std::move(internalName)) {}

Fertilizer::EditorNode::EditorNode(EditorGraph& graph, std::string title, std::string internalName, const rapidjson::Value& json)
        : graph(graph), id(Carrot::UUID::fromString(json["node_id"].GetString())), title(std::move(title)), internalName(std::move(internalName)) {
    setPosition(ImVec2(json["x"].GetFloat(), json["y"].GetFloat()));
    graph.reserveID(id);
}

bool Fertilizer::EditorNode::draw() {
    handlePosition();

    bool modified = false;
    u32 nodeID = graph.getEditorID(id);
    nodeSize = ed::GetNodeSize(nodeID);
    const ImVec2 nodePosition = ed::GetNodePosition(nodeID);

    ed::BeginNode(nodeID);

    ImGui::PushID(nodeID);

    ImGui::BeginVertical("node");

    // compensate for the 0 padding of nodes (used to render IO pins properly)
    ImGui::Dummy(EditorGraph::PaddingDummySize);
    ImGui::SameLine();
    modified |= renderHeaderWidgets();
    ImGui::SameLine();
    ImGui::Dummy(EditorGraph::PaddingDummySize);

    const float headerHeight = ImGui::GetCursorPosY() - nodePosition.y;

    ImGui::BeginHorizontal("content");

    ImGui::BeginVertical("inputs");
    ed::PushStyleVar(ed::StyleVar_PivotAlignment, ImVec2(0, 0.5f));
    ed::PushStyleVar(ed::StyleVar_PivotSize, ImVec2(0, 0));

    modified |= renderInputPins();
    ImGui::EndVertical();

    if (!inputs.empty()) {
        ImGui::Spring();
    }

    ImGui::BeginVertical("center");
    modified |= renderCenter();
    ImGui::EndVertical();

    if (!outputs.empty()) {
        ImGui::Spring();
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

    for (auto& func : postNodeDrawCallbacks) {

        func();
    }
    postNodeDrawCallbacks.clear();

    ImDrawList* pBackgroundDraw = ed::GetNodeBackgroundDrawList(graph.getEditorID(id));
    const auto borderWidth = ed::GetStyle().NodeBorderWidth;
    ImVec2 min = nodePosition;
    ImVec2 max = min;
    max.x += nodeSize.x;
    max.y += headerHeight;

    min.x += borderWidth;
    min.y += borderWidth;
    max.x -= borderWidth;
    max.y -= borderWidth;

    pBackgroundDraw->AddRectFilled(min, max, getHeaderColor(), ed::GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop);

    return modified;
}

void Fertilizer::EditorNode::handlePosition() {
    u32 nodeID = graph.getEditorID(id);

    if(updatePosition) {
        ed::SetNodePosition(nodeID, position);
    } else if (followingMouseUntilClick) {
        const ImVec2 mousePos = ImGui::GetMousePos();
        if (ed::IsActive() && ImGui::IsMousePosValid(&mousePos)) {
            ImVec2 nodePos;
            if (offsetFromMouse.has_value()) { // code asked for specific position relative to mouse
                nodePos.x = mousePos.x + offsetFromMouse->x;
                nodePos.y = mousePos.y + offsetFromMouse->y;
            } else { // header centered on mouse
                nodePos.x = mousePos.x - nodeSize.x/2;
                nodePos.y = mousePos.y - ImGui::GetTextLineHeight()/2;
            }
            ed::SetNodePosition(nodeID, nodePos);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                followingMouseUntilClick = false;
            }
        }
    }
    position = ed::GetNodePosition(nodeID);
    updatePosition = false;
}

bool Fertilizer::EditorNode::renderCenter() {
    return false;
}

bool Fertilizer::EditorNode::renderHeaderWidgets() {
    ImGui::Text("%s", title.c_str());
    return false;
}

void Fertilizer::EditorNode::renderSinglePin(bool isOutput, const Pin& pin) {
    const float lineHeight = ImGui::GetTextLineHeight();
    const float circleRadius = lineHeight/2;
    const float innerCircleRadius = lineHeight/2 - 1;

    bool isConnected = false;
    if (isOutput) {
        isConnected = graph.hasLinksStartingFrom(pin);
    } else {
        isConnected = graph.hasLinksLeadingTo(pin);
    }

    auto drawPinCircle = [&](ImVec2 pinCircleCursorPosition) {
        postNodeDrawCallbacks.emplaceBack([isConnected, innerCircleRadius, pinCircleCursorPosition, circleRadius]() {
            int segmentCount = 22;
            ImVec2 pos = pinCircleCursorPosition;
            const ed::Style& style = ed::GetStyle();
            ImDrawList* pDrawList = ImGui::GetWindowDrawList();

            pos.y += circleRadius;
            pDrawList->AddCircleFilled(pos, circleRadius, ImColor(style.Colors[ed::StyleColor_NodeBorder]), segmentCount);

            ImColor innerColor = ImGuiUtils::getCarrotColor();
            if (!isConnected) {
                innerColor.Value.x *= 0.3f;
                innerColor.Value.y *= 0.3f;
                innerColor.Value.z *= 0.3f;
            }
            pDrawList->AddCircleFilled(pos, innerCircleRadius, innerColor, segmentCount);
        });
    };

    ed::BeginPin(graph.getEditorID(pin.id), isOutput ? ed::PinKind::Output : ed::PinKind::Input);
    ImGui::BeginHorizontal(&pin.id);
    if (!isOutput) {
        ImGui::Dummy(ImVec2(circleRadius/2, 1));
    }
    ImGui::TextUnformatted(pin.name.c_str());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::GetStyle().ItemSpacing.x/2 /*remove some spacing, icon is too fat otherwise*/);
    const ImVec2 iconSize { lineHeight, lineHeight };
    if (auto* pProvider = graph.getImGuiTextures()) {
        if (auto icon = pProvider->getExpressionType(pin.getExpressionType())) {
            ImGui::Image(icon, iconSize);
        } else {
            ImGui::Dummy(iconSize);
        }
    } else {
        ImGui::Dummy(iconSize);
    }
    if (isOutput) {
        ImGui::Dummy(ImVec2(circleRadius/2, 1));
    }
    ImGui::EndHorizontal();

    if (isOutput) {
        ed::PinRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax() + ImVec2(circleRadius, 0));
    } else {
        ed::PinRect(ImGui::GetItemRectMin() - ImVec2(circleRadius, 0), ImGui::GetItemRectMax());
    }

    if (!isOutput) { // force pin to be on left border of node
        ImVec2 pinCirclePosition = ImGui::GetItemRectMin();
        drawPinCircle(pinCirclePosition);
    } else { // otherwise, we force the pin to be on right border of node, but this requires to compute the size of the pin first
        ImVec2 pinCirclePosition = ImGui::GetItemRectMax() - ImVec2(0.0f, circleRadius*2);
        drawPinCircle(pinCirclePosition);
    }
    ed::EndPin();
}

bool Fertilizer::EditorNode::renderInputPins() {
    for (auto& pin : inputs) {
        renderSinglePin(false, *pin);
    }
    return false;
}

bool Fertilizer::EditorNode::renderOutputPins() {
    for (auto& pin : outputs) {
        renderSinglePin(true, *pin);
    }
    return false;
}

Fertilizer::Input& Fertilizer::EditorNode::newInput(std::string name, Carrot::ExpressionType type) {
    auto result = std::make_shared<Input>(*this, type, inputs.size(), graph.nextID(), name);
    inputs.push_back(result);
    graph.registerPin(result);
    return *result;
}

Fertilizer::Output& Fertilizer::EditorNode::newOutput(std::string name, Carrot::ExpressionType type) {
    auto result = std::make_shared<Output>(*this, type, outputs.size(), graph.nextID(), name);
    outputs.push_back(result);
    graph.registerPin(result);
    return *result;
}

Fertilizer::EditorNode& Fertilizer::EditorNode::setPosition(ImVec2 position) {
    this->position = position;
    updatePosition = true;
    return *this;
}

Fertilizer::EditorNode& Fertilizer::EditorNode::setPosition(glm::vec2 position) {
    return setPosition(ImVec2{ position.x, position.y });
}

glm::vec2 Fertilizer::EditorNode::getPosition() const {
    return glm::vec2 { position.x, position.y };
}

glm::vec2 Fertilizer::EditorNode::getSize() const {
    return glm::vec2 { nodeSize.x, nodeSize.y };
}

void Fertilizer::EditorNode::followMouseUntilClick(std::optional<glm::vec2> offsetFromMouse) {
    followingMouseUntilClick = true;
    this->offsetFromMouse = offsetFromMouse;
}

Fertilizer::EditorNode::~EditorNode() {

}

ImColor Fertilizer::EditorNode::getHeaderColor() const {
    return ImColor(0.3f, 0.3f, 0.3f, 0.3f);
}

rapidjson::Value Fertilizer::EditorNode::toJSON(rapidjson::MemoryPoolAllocator<>& allocator) const {
    rapidjson::Value object(rapidjson::kObjectType);
    object.AddMember("x", position.x, allocator);
    object.AddMember("y", position.y, allocator);
    auto uuidStr = id.toString();
    object.AddMember("node_id", rapidjson::Value(uuidStr.c_str(), allocator), allocator);
    object.AddMember("node_type", rapidjson::Value(internalName.c_str(), allocator), allocator);

    auto serialised = serialiseToJSON(allocator);
    if(!serialised.IsNull() && serialised.IsObject()) {
        object.AddMember("extra", serialised, allocator);
    }
    return object;
}

std::vector<std::shared_ptr<Carrot::Expression>> Fertilizer::EditorNode::getExpressionsFromInput(std::unordered_set<Carrot::UUID>& activeLinks) const {
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

rapidjson::Value Fertilizer::Pin::toJSONReference(rapidjson::Document& document) const {
    auto result = rapidjson::Value(rapidjson::kObjectType);
    result.AddMember("node_id", Carrot::JSON::makeRef(owner.getID().toString()), document.GetAllocator());
    result.AddMember("pin_index", pinIndex, document.GetAllocator());
    result.AddMember("pin_type", rapidjson::StringRef(getType() == PinType::Input ? "input" : "output"), document.GetAllocator());
    return std::move(result);
}
