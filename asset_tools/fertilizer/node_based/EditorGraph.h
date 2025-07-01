//
// Created by jglrxavpok on 02/05/2021.
//

#pragma once

#include "core/utils/Containers.h"
#include <imgui_node_editor.h>
#include "EditorNode.h"
#include "core/memory/BidirectionalMap.hpp"
#include "core/utils/UUID.h"
#include "nodes/TerminalNodes.h"
#include "nodes/VariableNodes.h"
#include "nodes/Template.h"
#include <map>
#include <utility>
#include <core/containers/Vector.hpp>

namespace Fertilizer {
    namespace ed = ax::NodeEditor;

    class ParseError : public std::runtime_error {
    public:
        explicit ParseError(const std::string& _Message) : std::runtime_error(_Message.c_str()) {}

        explicit ParseError(const char* _Message) : std::runtime_error(_Message) {}
    };

    struct Link {
        Carrot::UUID id;
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

    template<typename NodeType> requires Fertilizer::IsNode<NodeType>
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

    struct ImGuiTexturesProvider {
        virtual ~ImGuiTexturesProvider() = default;
        virtual ImTextureID getExpressionType(const Carrot::ExpressionType& type) = 0;
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
        std::string name;
        std::vector<Link> links;
        std::unordered_set<Carrot::UUID> coloredLinkUUIDs;
        std::vector<TemporaryLabel> tmpLabels;
        bool zoomToContent = false;
        ed::EditorContext* g_Context = nullptr;
        std::unique_ptr<NodeLibraryMenu> rootMenu = nullptr;
        NodeLibraryMenu* currentMenu = nullptr;
        ImGuiTexturesProvider* pImguiTextures = nullptr;

        std::chrono::time_point<std::chrono::steady_clock> lastChangeTime{};
        bool hasUnsavedChanges = false;
        bool isFocused = false;

        // shortcuts
        bool cutQueued = false;
        bool copyQueued = false;
        bool pasteQueued = false;
        bool duplicateQueued = false;

        struct ClipboardLink {
            u32 nodeFrom = 0; // index into 'nodes' below
            u32 pinFrom = 0;

            u32 nodeTo = 0; // index into 'nodes' below
            u32 pinTo = 0;
        };

        // used to store nodes and links when user copies/cuts nodes and/or links, not intended for serialization
        struct Clipboard {
            rapidjson::MemoryPoolAllocator<> jsonAllocator;
            Carrot::Vector<rapidjson::Value> nodes;
            Carrot::Vector<ClipboardLink> links;

            glm::vec2 boundsMin{};
            glm::vec2 boundsMax{};
        };

        Clipboard clipboard;

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
        inline const static ImVec2 PaddingDummySize { 4.0f, 0.0f };

        explicit EditorGraph(std::string name);
        ~EditorGraph();

        Carrot::UUID nextID();
        uint32_t reserveID(const Carrot::UUID& id);
        uint32_t getEditorID(const Carrot::UUID& id);

        void draw();
        void tick(double deltaTime);

    public:
        void onCutShortcut();
        void onCopyShortcut();
        void onPasteShortcut();
        void onDuplicateShortcut();

    public:
        void addTemporaryLabel(const std::string& text);

        void recurseDrawNodeLibraryMenus(const NodeLibraryMenu& menu);

        ImGuiTexturesProvider* getImGuiTextures() { return pImguiTextures; };

        void setImGuiTexturesProvider(ImGuiTexturesProvider*);

    public:
        template<class T, typename... Args> requires IsNode<T>
        T& newNode(Args&&... args) {
            auto result = std::make_shared<T>(*this, args...);
            id2node[result->getID()] = result;
            if constexpr (std::is_base_of_v<Fertilizer::TerminalNode, T>) {
                terminalNodes.push_back(result);
            }
            hasUnsavedChanges = true;
            return *result;
        }

        void registerPin(std::shared_ptr<Pin> pin);
        void unregisterPin(std::shared_ptr<Pin> pin);
        void removeNode(const Fertilizer::EditorNode& node);
        void clear();

    public:
        bool hasLinksStartingFrom(const Fertilizer::Pin& from) const;
        bool hasLinksLeadingTo(const Fertilizer::Pin& to) const;
        std::vector<Fertilizer::Link> getLinksStartingFrom(const Fertilizer::Pin& from) const;
        std::vector<Fertilizer::Link> getLinksLeadingTo(const Fertilizer::Pin& to) const;
        const std::unordered_map<Carrot::UUID, std::shared_ptr<EditorNode>>& getNodes() { return id2node; };

        void addLink(const Fertilizer::Link& link);
        void removeLink(const Fertilizer::Link& link);
        void removeColoredLinks();
        void addColoredLink(const Carrot::UUID& linkID);

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

        template<typename NodeType> requires Fertilizer::IsNode<NodeType>
        void addToLibrary(const std::string& internalName, const std::string& title) {
            addToLibrary(internalName, title, std::move(std::make_unique<NodeInitialiser<NodeType>>()));
        }

    public:
        void resetChangeFlag();
        bool hasChanges() const;

        void markDirty();
        std::chrono::time_point<std::chrono::steady_clock> getLastChangeTime() const;

    public:
        EditorNode& loadSingleNode(const rapidjson::Value& nodeJSON);
        void loadFromJSON(const rapidjson::Value& json);

        rapidjson::Value toJSON(rapidjson::Document& document);

        std::vector<std::shared_ptr<Carrot::Expression>> generateExpressionsFromTerminalNodes(std::unordered_set<Carrot::UUID>& activeLinks) const;

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
inline Fertilizer::EditorNode& Fertilizer::NodeInitialiser<Fertilizer::TerminalNode>::operator()(Fertilizer::EditorGraph& graph, const rapidjson::Value& json) {
    throw std::runtime_error("SHOULD NOT HAPPEN");
}

template<>
inline Fertilizer::EditorNode& Fertilizer::NodeInitialiser<Fertilizer::TerminalNode>::operator()(Fertilizer::EditorGraph& graph) {
    throw std::runtime_error("SHOULD NOT HAPPEN");
}

template<>
inline Fertilizer::EditorNode& Fertilizer::NodeInitialiser<Fertilizer::VariableNode>::operator()(Fertilizer::EditorGraph& graph, const rapidjson::Value& json) {
    throw std::runtime_error("SHOULD NOT HAPPEN");
}

template<>
inline Fertilizer::EditorNode& Fertilizer::NodeInitialiser<Fertilizer::VariableNode>::operator()(Fertilizer::EditorGraph& graph) {
    throw std::runtime_error("SHOULD NOT HAPPEN");
}

template<typename NodeType> requires Fertilizer::IsNode<NodeType>
Fertilizer::EditorNode& Fertilizer::NodeInitialiser<NodeType>::operator()(Fertilizer::EditorGraph& graph, const rapidjson::Value& json) {
    return graph.newNode<NodeType>(json);
}

template<typename NodeType> requires Fertilizer::IsNode<NodeType>
Fertilizer::EditorNode& Fertilizer::NodeInitialiser<NodeType>::operator()(Fertilizer::EditorGraph& graph) {
    return graph.newNode<NodeType>();
}