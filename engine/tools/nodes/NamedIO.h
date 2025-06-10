//
// Created by jglrxavpok on 29/05/2021.
//

#pragma once
#include "../EditorNode.h"
#include "core/io/IO.h"
#include "core/utils/JSON.h"
#include <filesystem>
#include "NodeEditorUtils.h"

namespace Tools {
    struct NamedInputPlaceholderData {
        std::string name;
        std::uint32_t index;
    };

    template<bool isInput>
    class NamedIONode: public EditorNode {
    public:
        explicit NamedIONode(Tools::EditorGraph& graph): EditorNode(graph, isInput ? "Named Input" : "Named Output", isInput ? "named_input" : "named_output") {
            name = isInput ? "NamedInput" : "NamedOutput";
            load();
        }

        explicit NamedIONode(Tools::EditorGraph& graph, const rapidjson::Value& json): EditorNode(graph, isInput ? "Named Input" : "Named Output", isInput ? "named_input" : "named_output", json) {
            name = json["extra"]["name"].GetString();
            type = Carrot::ExpressionTypes::fromName(json["extra"]["type"].GetString());
            dimensions = json["extra"]["dimensions"].GetInt64();
            load();
        }

    public:
        Carrot::ExpressionType getType() const { return type; }
        const std::string& getIOName() const { return name; }
        uint32_t getDimensionCount() const { return dimensions; };

    public:
        rapidjson::Value serialiseToJSON(rapidjson::MemoryPoolAllocator<>& allocator) const override {
            return std::move(rapidjson::Value(rapidjson::kObjectType)
                                     .AddMember("name", Carrot::JSON::makeRef(name), allocator)
                                     .AddMember("type", Carrot::JSON::makeRef(type.name()), allocator)
                                     .AddMember("dimensions", dimensions, allocator)
            );
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex, std::unordered_set<Carrot::UUID>& activeLinks) const override {
            if constexpr (isInput) {
                auto placeholderData = std::make_shared<NamedInputPlaceholderData>();
                placeholderData->name = getIOName();
                placeholderData->index = outputIndex;
                return std::make_shared<Carrot::PlaceholderExpression>(placeholderData, getType());
            } else {
                return getExpressionsFromInput(activeLinks)[outputIndex];
            }
        }

        bool renderCenter() override {
            bool modified = false;
            ImGui::SetNextItemWidth(200);
            if(ImGui::InputTextWithHint("##name", "IO name...", nameImGuiBuffer, sizeof(nameImGuiBuffer))) {
                modified = true;
                name = nameImGuiBuffer;
            }
            ImGui::SetNextItemWidth(200);
            if(Tools::NodeEditor::BeginNodeCombo("Type##template_node_type", selectableTypes[selectedTypeIndex].name().c_str(), 0)) {
                uint32_t newSelectedTypeIndex = selectedTypeIndex;
                for (int i = 0; i < selectableTypes.size(); ++i) {
                    if(ImGui::Selectable(selectableTypes[i].name().c_str())) {
                        newSelectedTypeIndex = i;
                    }
                }

                if(selectedTypeIndex != newSelectedTypeIndex) {
                    breakConnections();
                    selectedTypeIndex = newSelectedTypeIndex;
                    type = selectableTypes[selectedTypeIndex];

                    if constexpr (isInput) {
                        for(const auto& output : outputs) {
                            output->expressionType = type;
                        }
                    } else {
                        for(const auto& input : inputs) {
                            input->expressionType = type;
                        }
                    }
                    modified = true;
                }

                Tools::NodeEditor::EndNodeCombo();
            }

            ImGui::SetNextItemWidth(200);
            int newDimensionCount = dimensions;
            if(ImGui::DragInt("Dimension Count", &newDimensionCount, 1.0f, 1, 4)) {
                if(newDimensionCount != dimensions) {
                    if(newDimensionCount < dimensions) {
                        breakConnections(newDimensionCount);
                        if constexpr (isInput) {
                            outputs.resize(newDimensionCount);
                        } else {
                            inputs.resize(newDimensionCount);
                        }
                    } else {
                        // add new pins
                        for (int i = 0; i < newDimensionCount-dimensions; ++i) {
                            if constexpr (isInput) {
                                newOutput("Pin #"+std::to_string(i+1 + dimensions), type);
                            } else {
                                newInput("Pin #"+std::to_string(i+1 + dimensions), type);
                            }
                        }
                    }
                    dimensions = newDimensionCount;
                    modified = true;
                }
            }
            return true;
        }

    private:
        void breakConnections(uint32_t startingFromPinIndex = 0) {
            size_t size = isInput ? outputs.size() : inputs.size();
            for (int i = startingFromPinIndex; i < size; ++i) {
                if constexpr (isInput) {
                    for(const auto& link : graph.getLinksStartingFrom(*outputs[i])) {
                        graph.removeLink(link);
                    }
                } else {
                    for(const auto& link : graph.getLinksLeadingTo(*inputs[i])) {
                        graph.removeLink(link);
                    }
                }
            }
        }

        void load() {
            // add pins
            for (int i = 0; i < dimensions; ++i) {
                if constexpr (isInput) {
                    newOutput("Pin #"+std::to_string(i+1), type);
                } else {
                    newInput("Pin #"+std::to_string(i+1), type);
                }
            }

            Carrot::strcpy(nameImGuiBuffer, name.c_str());
        }

    private:
        inline static std::vector<Carrot::ExpressionType> selectableTypes = {Carrot::ExpressionTypes::Float, Carrot::ExpressionTypes::Bool};
        uint32_t selectedTypeIndex = 0;
        char nameImGuiBuffer[128] = {'\0'};
        std::string name;
        uint32_t dimensions = 1;
        Carrot::ExpressionType type = Carrot::ExpressionTypes::Float;
    };

    using NamedInputNode = NamedIONode<true>;
    using NamedOutputNode = NamedIONode<false>;
}