//
// Created by jglrxavpok on 02/05/2021.
//

#pragma once

#include "engine/Engine.h"
#include "imgui_node_editor.h"
#include <rapidjson/document.h>
#include "core/utils/UUID.h"
#include "core/expressions/Expressions.h"

namespace Tools {
    namespace ed = ax::NodeEditor;

    class EditorGraph;

    struct Pin;
    class EditorNode;

    enum class PinType {
        Input,
        Output,
    };

    struct Pin: public std::enable_shared_from_this<Pin> {
        EditorNode& owner;
        Carrot::UUID id;
        std::string name;
        uint32_t pinIndex = 0;
        Carrot::ExpressionType expressionType;

        explicit Pin(EditorNode& owner, Carrot::ExpressionType exprType, std::uint32_t pinIndex, Carrot::UUID id, std::string n): owner(owner), expressionType(exprType), pinIndex(pinIndex), id(id), name(std::move(n)) {}

        virtual PinType getType() const = 0;
        Carrot::ExpressionType getExpressionType() { return expressionType; };

        rapidjson::Value toJSONReference(rapidjson::Document& document) const;
    };

    struct Input: public Pin {
        explicit Input(EditorNode& owner, Carrot::ExpressionType exprType, std::uint32_t pinIndex, Carrot::UUID id, std::string n): Pin(owner, exprType, pinIndex, id, std::move(n)) {}

        virtual PinType getType() const override { return PinType::Input; };
    };
    struct Output: public Pin {
        explicit Output(EditorNode& owner, Carrot::ExpressionType exprType, std::uint32_t pinIndex, Carrot::UUID id, std::string n): Pin(owner, exprType, pinIndex, id, std::move(n)) {}

        virtual PinType getType() const override { return PinType::Output; };
    };

    class EditorNode: public std::enable_shared_from_this<EditorNode> {
    protected:
        std::vector<std::shared_ptr<Input>> inputs;
        std::vector<std::shared_ptr<Output>> outputs;

        Input& newInput(std::string name, Carrot::ExpressionType type);
        Output& newOutput(std::string name, Carrot::ExpressionType type);

        [[nodiscard]] std::vector<std::shared_ptr<Carrot::Expression>> getExpressionsFromInput() const;
        std::string title;
        std::string internalName;
        EditorGraph& graph;

    private:
        Carrot::UUID id;
        bool updatePosition = true;
        bool followingMouseUntilClick = false;
        ImVec2 position{};

    public:
        explicit EditorNode(EditorGraph& graph, std::string title, std::string internalName);

        /// Deserialisation
        explicit EditorNode(EditorGraph& graph, std::string title, std::string internalName, const rapidjson::Value& json);
        virtual ~EditorNode();

        EditorNode& setPosition(ImVec2 position);
        void followMouseUntilClick();

        // return true if the contents of the node was changed, can be used to reload previews, show that there are unsaved changes, etc
        bool draw();

        // return true if the contents of the node was changed, can be used to reload previews, show that there are unsaved changes, etc
        // Draws the title bar of the node
        virtual bool renderHeaderWidgets();

        // return true if the contents of the node was changed, can be used to reload previews, show that there are unsaved changes, etc
        // Draws the interior of the node
        virtual bool renderCenter();

        /// Render the pins for this node. Will be called while already being at the correct location, just draw your ImGui/node editor widgets
        /// Return true if the contents of the node have been modified (like renderCenter & renderHeaderWidgets)
        virtual bool renderInputPins();
        virtual bool renderOutputPins();

        const std::string& getTitle() const { return title; };
        const std::string& getInternalName() const { return internalName; };

        const std::vector<std::shared_ptr<Input>>& getInputs() const { return inputs; };
        const std::vector<std::shared_ptr<Output>>& getOutputs() const { return outputs; };

    public:
        const Carrot::UUID& getID() const { return id; };

    public:
        virtual rapidjson::Value serialiseToJSON(rapidjson::Document& doc) const {
            return rapidjson::Value();
        }
        rapidjson::Value toJSON(rapidjson::Document& doc) const;

        virtual std::shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex) const = 0;

    private:
        ImDrawListSplitter drawListSplitter;
    };
}
