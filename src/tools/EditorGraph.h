//
// Created by jglrxavpok on 02/05/2021.
//

#pragma once

#include "engine/Engine.h"
#include "imgui_node_editor.h"
#include "EditorNode.h"
#include "engine/memory/BidirectionalMap.hpp"

namespace Tools {
    namespace ed = ax::NodeEditor;

    class ParseError : public std::runtime_error {
    public:
        explicit ParseError(const std::string& _Message) : std::runtime_error(_Message.c_str()) {}

        explicit ParseError(const char* _Message) : std::runtime_error(_Message) {}
    };

    struct Link {
        uint32_t id = 0;
        std::weak_ptr<Pin> inputPin;
        std::weak_ptr<Pin> outputPin;
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
        uint32_t uniqueID = 1;
        unordered_map<uint32_t, shared_ptr<Pin>> id2pin;
        unordered_map<uint32_t, shared_ptr<EditorNode>> id2node;

        Carrot::Engine& engine;
        std::string name;
        vector<Link> links;
        ed::EditorContext* g_Context = nullptr;

    public:
        explicit EditorGraph(Carrot::Engine& engine, std::string name);
        ~EditorGraph();

        uint32_t nextID() { return uniqueID++; };

        void onFrame(size_t frameIndex);

    public:
        template<class T, typename... Args> requires IsNode<T>
        T& newNode(Args&&... args) {
            auto result = make_shared<T>(*this, args...);
            id2node[result->getID().Get()] = result;
            return *result;
        }

        void registerPin(ed::PinId id, shared_ptr<Pin> pin);
        void unregisterPin(ed::PinId id);

        bool canLink(Pin& pinA, Pin& pinB);

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