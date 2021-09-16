//
// Created by jglrxavpok on 02/05/2021.
//

#pragma once

#include "engine/Engine.h"
#include "engine/utils/Containers.h"
#include "imgui_node_editor.h"
#include "EditorNode.h"
#include "engine/memory/BidirectionalMap.hpp"
#include "engine/utils/UUID.h"
#include "nodes/TerminalNodes.h"
#include "nodes/VariableNodes.h"
#include "nodes/Template.h"
#include <utility>

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
        IncompatibleTypes,
    };

    enum class NodeValidity {
        Possible,
        TerminalOfSameTypeAlreadyExists,
    };

    template<class T>
    concept IsNode = std::is_base_of_v<EditorNode, T>;

    template<class T>
    concept IsTerminalNode = std::is_base_of_v<TerminalNode, T>;

    struct NodeInitialiserBase {
        virtual EditorNode& operator()(EditorGraph& graph, const rapidjson::Value& json) = 0;
        virtual EditorNode& operator()(EditorGraph& graph) = 0;
        inline virtual NodeValidity getValidity(EditorGraph& graph) const { return NodeValidity::Possible; };

        virtual ~NodeInitialiserBase() {};
    };

    template<typename NodeType> requires Tools::IsNode<NodeType>
    struct NodeInitialiser: NodeInitialiserBase {
        EditorNode& operator()(EditorGraph& graph, const rapidjson::Value& json) override;

        EditorNode& operator()(EditorGraph& graph) override;
    };

    struct TemporaryLabel {
        double remainingTime = 5.0f;
        std::string text;

        inline explicit TemporaryLabel(const std::string& text): text(std::move(text)) {};
    };

    class NodeLibraryMenu {
    public:
        explicit NodeLibraryMenu(std::string name, NodeLibraryMenu* parent = nullptr): name(std::move(name)), parent(parent) {}

        NodeLibraryMenu& newChild(const std::string& name) {
            submenus.emplace_back(NodeLibraryMenu(name, this));
            return submenus.back();
        }

        void addEntry(const std::string& name) {
            entries.push_back(name);
        }

        const std::string& getName() const {
            return name;
        }

        const std::vector<std::string>& getEntries() const {
            return entries;
        }

        const std::list<NodeLibraryMenu>& getSubmenus() const {
            return submenus;
        }

        std::list<NodeLibraryMenu>& getSubmenus() {
            return submenus;
        }

        NodeLibraryMenu* getParent() const {
            return parent;
        }

    private:
        NodeLibraryMenu* parent = nullptr;
        std::string name;
        std::list<NodeLibraryMenu> submenus;
        std::vector<std::string> entries;
    };

    class NodeLibraryMenu;

    struct ImGuiTextures {
        std::unique_ptr<Carrot::Render::Texture> empty = nullptr;
        std::unordered_map<Carrot::ExpressionType, std::unique_ptr<Carrot::Render::Texture>> expressionTypes;

        ImTextureID getExpressionType(const Carrot::ExpressionType& type) {
            return expressionTypes[type]->getImguiID();
        }
    };

    class EditorGraph {
    private:
        std::map<std::string, std::unique_ptr<NodeInitialiserBase>> nodeLibrary;
        std::map<std::string, std::string> internalName2title;
        ed::Config config{};
        std::unordered_map<Carrot::UUID, std::shared_ptr<Pin>> id2pin;
        std::unordered_map<Carrot::UUID, std::shared_ptr<EditorNode>> id2node;
        std::unordered_map<Carrot::UUID, std::uint32_t> uuid2id;
        std::unordered_map<uint32_t, Carrot::UUID> id2uuid;
        std::vector<std::weak_ptr<TerminalNode>> terminalNodes;
        std::uint32_t uniqueID = 1;
        Carrot::Engine& engine;
        std::string name;
        std::vector<Link> links;
        std::vector<TemporaryLabel> tmpLabels;
        bool zoomToContent = false;
        ed::EditorContext* g_Context = nullptr;
        std::unique_ptr<NodeLibraryMenu> rootMenu = nullptr;
        NodeLibraryMenu* currentMenu = nullptr;
        ImGuiTextures imguiTextures;
        bool hasUnsavedChanges = false;

        std::uint32_t nextFreeEditorID();
        static void showLabel(const std::string& text, ImColor color = ImColor(255, 32, 32, 255));
        void handleLinkCreation(std::shared_ptr<Pin> pinA, std::shared_ptr<Pin> pinB);

    private:
        struct TemplateInit: public NodeInitialiserBase {
            inline EditorNode& operator()(EditorGraph& graph, const rapidjson::Value& json) override {
                return graph.newNode<TemplateNode>(json);
            };

            EditorNode& operator()(EditorGraph& graph) override {
                throw std::runtime_error("not allowed");
            };

            NodeValidity getValidity(EditorGraph& graph) const override {
                return NodeValidity::Possible;
            };
        };

    public:
        explicit EditorGraph(Carrot::Engine& engine, std::string name);
        ~EditorGraph();

        uuids::uuid nextID();
        uint32_t reserveID(const Carrot::UUID& id);
        uint32_t getEditorID(const Carrot::UUID& id);

        void onFrame(Carrot::Render::Context renderContext);
        void tick(double deltaTime);

    public:
        void addTemporaryLabel(const std::string& text);

        void recurseDrawNodeLibraryMenus(const NodeLibraryMenu& menu);

        ImGuiTextures& getImGuiTextures() { return imguiTextures; };

    public:
        template<class T, typename... Args> requires IsNode<T>
        T& newNode(Args&&... args) {
            auto result = std::make_shared<T>(*this, args...);
            id2node[result->getID()] = result;
            if constexpr (std::is_base_of_v<Tools::TerminalNode, T>) {
                terminalNodes.push_back(result);
            }
            hasUnsavedChanges = true;
            return *result;
        }

        void registerPin(std::shared_ptr<Pin> pin);
        void unregisterPin(std::shared_ptr<Pin> pin);
        void removeNode(const Tools::EditorNode& node);
        void clear();

    public:
        std::vector<Tools::Link> getLinksStartingFrom(const Tools::Pin& from) const;
        std::vector<Tools::Link> getLinksLeadingTo(const Tools::Pin& to) const;
        const std::unordered_map<Carrot::UUID, std::shared_ptr<EditorNode>>& getNodes() { return id2node; };

        void addLink(Tools::Link link);
        void removeLink(const Tools::Link& link);

        LinkPossibility canLink(Pin& from, Pin& to);


    public:
        void addToLibrary(const std::string& internalName, const std::string& title, std::unique_ptr<NodeInitialiserBase>&& nodeCreator) {
            addToLibraryNoMenu(internalName, title, std::move(nodeCreator));
            currentMenu->addEntry(internalName);
        }

        void addToLibraryNoMenu(const std::string& internalName, const std::string& title, std::unique_ptr<NodeInitialiserBase>&& nodeCreator) {
            nodeLibrary[internalName] = std::move(nodeCreator);
            internalName2title[internalName] = title;
        }

        void addTemplateSupport() {
            addToLibraryNoMenu("template", "Templates", std::make_unique<TemplateInit>());
        }

        void addTemplatesToLibrary();

        template<TerminalNodeType NodeType>
        void addToLibrary() {
            struct TerminalNodeInit: public NodeInitialiser<TerminalNode> {
                inline EditorNode& operator()(EditorGraph& graph, const rapidjson::Value& json) override {
                    return graph.newNode<TerminalNode>(NodeType, json);
                };

                inline EditorNode& operator()(EditorGraph& graph) override {
                    return graph.newNode<TerminalNode>(NodeType);
                };

                inline NodeValidity getValidity(EditorGraph& graph) const override {
                    auto terminalOfSameType = std::find_if(WHOLE_CONTAINER(graph.terminalNodes), [&](const auto& n) {
                        auto locked = n.lock();
                        return locked != nullptr && locked->getTerminalType() == NodeType;
                    });
                    if(terminalOfSameType != graph.terminalNodes.end()) {
                        return NodeValidity::TerminalOfSameTypeAlreadyExists;
                    }
                    return NodeValidity::Possible;
                };
            };
            addToLibrary(TerminalNode::getInternalName(NodeType), TerminalNode::getTitle(NodeType), std::move(std::make_unique<TerminalNodeInit>()));
        }

        template<VariableNodeType NodeType>
        void addVariableToLibrary() {
            struct VariableNodeInit: public NodeInitialiser<VariableNode> {
                inline EditorNode& operator()(EditorGraph& graph, const rapidjson::Value& json) override {
                    return graph.newNode<VariableNode>(NodeType, json);
                };

                inline EditorNode& operator()(EditorGraph& graph) override {
                    return graph.newNode<VariableNode>(NodeType);
                };
            };
            addToLibrary(VariableNode::getInternalName(NodeType), VariableNode::getTitle(NodeType), std::move(std::make_unique<VariableNodeInit>()));
        }

        template<typename NodeType> requires Tools::IsNode<NodeType>
        void addToLibrary(const std::string& internalName, const std::string& title) {
            addToLibrary(internalName, title, std::move(std::make_unique<NodeInitialiser<NodeType>>()));
        }

    public:
        void resetChangeFlag();
        bool hasChanges() const;

    public:
        Carrot::Engine& getEngine() {
            return engine;
        }

    public:
        void loadFromJSON(const rapidjson::Value& json);

        rapidjson::Value toJSON(rapidjson::Document& document);

        std::vector<std::shared_ptr<Carrot::Expression>> generateExpressionsFromTerminalNodes() const;

        friend class NodeLibraryMenuScope;
    };

    class NodeLibraryMenuScope {
    public:
        explicit NodeLibraryMenuScope(std::string name, EditorGraph* graph): graph(graph), menu(graph->currentMenu->newChild(name)) {
            graph->currentMenu = &menu;
        }

        ~NodeLibraryMenuScope() {
            graph->currentMenu = menu.getParent();
        }

    private:
        NodeLibraryMenu& menu;
        EditorGraph* graph;
    };
}

template<>
inline Tools::EditorNode& Tools::NodeInitialiser<Tools::TerminalNode>::operator()(Tools::EditorGraph& graph, const rapidjson::Value& json) {
    throw std::runtime_error("SHOULD NOT HAPPEN");
}

template<>
inline Tools::EditorNode& Tools::NodeInitialiser<Tools::TerminalNode>::operator()(Tools::EditorGraph& graph) {
    throw std::runtime_error("SHOULD NOT HAPPEN");
}

template<>
inline Tools::EditorNode& Tools::NodeInitialiser<Tools::VariableNode>::operator()(Tools::EditorGraph& graph, const rapidjson::Value& json) {
    throw std::runtime_error("SHOULD NOT HAPPEN");
}

template<>
inline Tools::EditorNode& Tools::NodeInitialiser<Tools::VariableNode>::operator()(Tools::EditorGraph& graph) {
    throw std::runtime_error("SHOULD NOT HAPPEN");
}

template<typename NodeType> requires Tools::IsNode<NodeType>
Tools::EditorNode& Tools::NodeInitialiser<NodeType>::operator()(Tools::EditorGraph& graph, const rapidjson::Value& json) {
    return graph.newNode<NodeType>(json);
}

template<typename NodeType> requires Tools::IsNode<NodeType>
Tools::EditorNode& Tools::NodeInitialiser<NodeType>::operator()(Tools::EditorGraph& graph) {
    return graph.newNode<NodeType>();
}