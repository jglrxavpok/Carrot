//
// Created by jglrxavpok on 02/05/2021.
//

#pragma once

#include "engine/Engine.h"
#include "imgui_node_editor.h"
#include <rapidjson/document.h>

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
        ed::PinId id;
        std::string name;
        uint32_t pinIndex = 0;

        explicit Pin(EditorNode& owner, std::uint32_t pinIndex, ed::PinId id, std::string n): owner(owner), pinIndex(pinIndex), id(id), name(std::move(n)) {}

        virtual PinType getType() const = 0;

        rapidjson::Value toJSONReference(rapidjson::Document& document);
    };

    struct Input: public Pin {
        explicit Input(EditorNode& owner, std::uint32_t pinIndex, ed::PinId id, std::string n): Pin(owner, pinIndex, id, std::move(n)) {}

        virtual PinType getType() const override { return PinType::Input; };
    };
    struct Output: public Pin {
        explicit Output(EditorNode& owner, std::uint32_t pinIndex, ed::PinId id, std::string n): Pin(owner, pinIndex, id, std::move(n)) {}

        virtual PinType getType() const override { return PinType::Output; };
    };

    class EditorNode: public std::enable_shared_from_this<EditorNode> {
    protected:
        std::vector<shared_ptr<Input>> inputs;
        std::vector<shared_ptr<Output>> outputs;

        Input& newInput(std::string name);
        Output& newOutput(std::string name);

    private:
        ed::NodeId id = 0;
        EditorGraph& graph;
        std::string title;
        std::string internalName;
        bool updatePosition = true;
        ImVec2 position{};

    public:
        explicit EditorNode(EditorGraph& graph, std::string title, std::string internalName);

        /// Deserialisation
        explicit EditorNode(EditorGraph& graph, std::string title, std::string internalName, const rapidjson::Value& json);
        virtual ~EditorNode();

        EditorNode& setPosition(ImVec2 position);

        void draw();
        virtual void renderCenter() {};

        const std::string& getTitle() const { return title; };
        const std::string& getInternalName() const { return internalName; };

        const std::vector<std::shared_ptr<Input>>& getInputs() const { return inputs; };
        const std::vector<std::shared_ptr<Output>>& getOutputs() const { return outputs; };

    public:
        const ed::NodeId& getID() { return id; };
        const std::uint32_t highestUsedID() const;

    public:
        virtual void deserialiseFromJSON(const rapidjson::Value& json) {}

        virtual rapidjson::Value serialiseToJSON(rapidjson::Document& doc) {
            return rapidjson::Value();
        }
        rapidjson::Value toJSON(rapidjson::Document& doc);

    private:

    };
}
