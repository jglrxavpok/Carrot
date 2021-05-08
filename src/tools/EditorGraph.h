//
// Created by jglrxavpok on 02/05/2021.
//

#pragma once

#include "engine/Engine.h"
#include "imgui_node_editor.h"
#include "EditorNode.h"
#include "engine/memory/BidirectionalMap.hpp"
#include "engine/utils/UUID.h"

namespace Tools {
    namespace ed = ax::NodeEditor;

    class ParseError : public std::runtime_error {
    public:
        explicit ParseError(const std::string& _Message) : std::runtime_error(_Message.c_str()) {}

        explicit ParseError(const char* _Message) : std::runtime_error(_Message) {}
    };

    struct Link {
        uuids::uuid id = Carrot::randomUUID();
        std::weak_ptr<Pin> from;
        std::weak_ptr<Pin> to;
    };

    enum class LinkPossibility {
        Possible,
        TooManyInputs,
        CyclicalGraph,
        CannotLinkToSelf,
    };

    template<class T>
    concept IsNode = std::is_base_of_v<EditorNode, T>;

    struct NodeInitialiserBase {
        virtual EditorNode& operator()(EditorGraph& graph, const rapidjson::Value& json) = 0;
        virtual EditorNode& operator()(EditorGraph& graph) = 0;

        virtual ~NodeInitialiserBase() {};
    };

    template<typename NodeType> requires Tools::IsNode<NodeType>
    struct NodeInitialiser: NodeInitialiserBase {
        EditorNode& operator()(EditorGraph& graph, const rapidjson::Value& json) override;

        EditorNode& operator()(EditorGraph& graph) override;
    };

    class EditorGraph {
    private:
        std::map<std::string, std::unique_ptr<NodeInitialiserBase>> nodeLibrary;
        std::map<std::string, std::string> internalName2title;
        ed::Config config{};
        unordered_map<Carrot::UUID, shared_ptr<Pin>> id2pin;
        unordered_map<Carrot::UUID, shared_ptr<EditorNode>> id2node;
        unordered_map<Carrot::UUID, uint32_t> uuid2id;
        unordered_map<uint32_t, Carrot::UUID> id2uuid;
        uint32_t uniqueID = 1;
        Carrot::Engine& engine;
        std::string name;
        vector<Link> links;
        ed::EditorContext* g_Context = nullptr;

        uint32_t nextFreeEditorID();
        void showLabel(const std::string& text, ImColor color = ImColor(255, 32, 32, 255));
        void handleLinkCreation(std::shared_ptr<Pin> pinA, std::shared_ptr<Pin> pinB);

    public:
        explicit EditorGraph(Carrot::Engine& engine, std::string name);
        ~EditorGraph();

        uuids::uuid nextID();
        uint32_t reserveID(const Carrot::UUID& id);
        uint32_t getEditorID(const Carrot::UUID& id);

        void onFrame(size_t frameIndex);

    public:
        template<class T, typename... Args> requires IsNode<T>
        T& newNode(Args&&... args) {
            auto result = make_shared<T>(*this, args...);
            id2node[result->getID()] = result;
            return *result;
        }

        void registerPin(shared_ptr<Pin> pin);
        void unregisterPin(shared_ptr<Pin> pin);
        void unregisterNode(const Tools::EditorNode& node);

        LinkPossibility canLink(Pin& from, Pin& to);

        void removeNode(EditorNode& node);

        void clear();

    public:
        void addToLibrary(const std::string& internalName, const std::string& title, std::unique_ptr<NodeInitialiserBase>&& nodeCreator) {
            nodeLibrary[internalName] = std::move(nodeCreator);
            internalName2title[internalName] = title;
        }

        template<typename NodeType> requires Tools::IsNode<NodeType>
        void addToLibrary(const std::string& internalName, const std::string& title) {
            addToLibrary(internalName, title, std::move(make_unique<NodeInitialiser<NodeType>>()));
        }

    public:
        void loadFromJSON(const rapidjson::Value& json);

        rapidjson::Value toJSON(rapidjson::Document& document);
    };
}

template<typename NodeType> requires Tools::IsNode<NodeType>
Tools::EditorNode& Tools::NodeInitialiser<NodeType>::operator()(Tools::EditorGraph& graph, const rapidjson::Value& json) {
    return graph.newNode<NodeType>(json);
}

template<typename NodeType> requires Tools::IsNode<NodeType>
Tools::EditorNode& Tools::NodeInitialiser<NodeType>::operator()(Tools::EditorGraph& graph) {
    return graph.newNode<NodeType>();
}