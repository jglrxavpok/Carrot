//
// Created by jglrxavpok on 02/05/2021.
//

#include "node_based/EditorGraph.h"

#include <utility>
#include <queue>
#include "nodes/Arithmetics.hpp"
#include "nodes/Constants.hpp"
#include <core/utils/ImGuiUtils.hpp>
#include <core/utils/Containers.h>
#include <filesystem>
#include <iostream>
#include <core/utils/stringmanip.h>

namespace ed = ax::NodeEditor;

Fertilizer::EditorGraph::EditorGraph(std::string name): name(std::move(name)) {
    config.NavigateButtonIndex = 2; // pan graph with middle button (left = select, right = popup menu)
    g_Context = ed::CreateEditor(&config);

    ed::SetCurrentEditor(g_Context);

    rootMenu = std::make_unique<NodeLibraryMenu>("<root>");
    currentMenu = rootMenu.get();

    TODO;
    /*
    imguiTextures.expressionTypes[Carrot::ExpressionTypes::Bool] = std::make_unique<Carrot::Render::Texture>(engine.getVulkanDriver(), "resources/textures/icons/boolean.png");
    imguiTextures.expressionTypes[Carrot::ExpressionTypes::Float] = std::make_unique<Carrot::Render::Texture>(engine.getVulkanDriver(), "resources/textures/icons/float.png");
    */
}

void Fertilizer::EditorGraph::draw() {
    ed::SetCurrentEditor(g_Context);
    ed::EnableShortcuts(true);

    ed::Begin(name.c_str());

    for(auto& [id, node] : id2node) {
        if (node->draw()) {
            markDirty();
        }
    }

    for(const auto& link : links) {
        if(auto input = link.to.lock()) {
            if(auto output = link.from.lock()) {
                const u32 linkID = getEditorID(link.id);
                ImColor linkColor = IM_COL32_BLACK;
                if (coloredLinkUUIDs.contains(link.id)) {
                    linkColor = ImGuiUtils::getCarrotColor();
                }
                ed::Link(linkID, getEditorID(output->id), getEditorID(input->id), linkColor);
            }
        }
    }

    if(ed::BeginCreate()) {
        ed::PinId input;
        ed::PinId output;

        if(ed::QueryNewLink(&input, &output)) {
            if(input && output) {
                auto& pinA = id2pin[id2uuid[input.Get()]];
                auto& pinB = id2pin[id2uuid[output.Get()]];
                if(pinA && pinB) {
                    handleLinkCreation(pinA, pinB);
                }
            }
        }
    }
    ed::EndCreate();

    if(ed::BeginDelete()) {
        ed::LinkId linkToDelete;
        while(ed::QueryDeletedLink(&linkToDelete)) {
            auto link = std::find_if(WHOLE_CONTAINER(links), [&](const auto& link) { return linkToDelete.Get() == uuid2id[link.id]; });
            if(link != links.end() && ed::AcceptDeletedItem()) {
                removeLink(*link);
            }
        }

        ed::NodeId nodeToDelete;
        while(ed::QueryDeletedNode(&nodeToDelete)) {
            auto node = id2node.find(id2uuid[nodeToDelete.Get()]);
            if(node != id2node.end()) {
                removeNode(*(node->second));
            }
        }

        ed::EndDelete();
    }

    if(zoomToContent) {
        ed::NavigateToContent();
        zoomToContent = false;
    }

    // shortcut handling
    // used for cut functionality
    Carrot::Vector<EditorNode*> copiedNodes;
    Carrot::Vector<Link*> copiedLinks;

    auto copyToClipboard = [&](Clipboard& clipboard) {
        Carrot::Vector<ed::NodeId> selectedGraphNodes;
        selectedGraphNodes.resize(ed::GetSelectedObjectCount());

        int nodeCount = ed::GetSelectedNodes(selectedGraphNodes.data(), static_cast<int>(selectedGraphNodes.size()));

        selectedGraphNodes.resize(nodeCount);

        if (nodeCount == 0) {
            return;
        }

        clipboard.links.clear();
        clipboard.nodes.clear();

        clipboard.boundsMin = { +INFINITY, +INFINITY };
        clipboard.boundsMax = { -INFINITY, -INFINITY };

        std::unordered_map<EditorNode*, u32> remap;
        std::unordered_set<EditorNode*> copiedNodesSet;
        for (const ed::NodeId& nodeID : selectedGraphNodes) {
            auto iter = this->id2uuid.find(nodeID.Get());
            if(iter == id2uuid.end()) {
                continue;
            }

            auto iter2 = id2node.find(iter->second);
            if(iter2 == id2node.end()) {
                continue;
            }

            EditorNode* pNode = iter2->second.get();
            remap[pNode] = static_cast<u32>(clipboard.nodes.size());
            clipboard.nodes.emplaceBack(pNode->toJSON(clipboard.jsonAllocator));
            clipboard.boundsMin = glm::min(clipboard.boundsMin, pNode->getPosition());
            clipboard.boundsMax = glm::max(clipboard.boundsMax, pNode->getPosition() + pNode->getSize());

            copiedNodes.emplaceBack(pNode);
            copiedNodesSet.insert(pNode);
        }

        for (auto& graphLink : links) {
            // if one of the lock() returns nullptr below, this means we are looking at a link referencing a node that was just deleted
            // unlikely, but could happen if the user is quick enough
            if (auto pPinFrom = graphLink.from.lock()) {
                if (auto pPinTo = graphLink.to.lock()) {
                    if (!copiedNodesSet.contains(&pPinFrom->owner)) {
                        continue;
                    }
                    if (!copiedNodesSet.contains(&pPinTo->owner)) {
                        continue;
                    }
                    ClipboardLink& link = clipboard.links.emplaceBack();
                    link.nodeFrom = remap[&pPinFrom->owner];
                    link.nodeTo = remap[&pPinTo->owner];
                    link.pinFrom = pPinFrom->pinIndex;
                    link.pinTo = pPinTo->pinIndex;
                    copiedLinks.emplaceBack(&graphLink);
                }
            }
        }
    };

    auto pasteFromClipboard = [&](Clipboard& clipboard) {
        Carrot::Vector<EditorNode*> remap; // works because the data structure for nodes is stable
        remap.ensureReserve(clipboard.nodes.size());
        const glm::vec2 center = (clipboard.boundsMin + clipboard.boundsMax) / 2.0f;
        for (const auto& serializedNode : clipboard.nodes) {
            rapidjson::Value copy{};
            copy.CopyFrom(serializedNode, clipboard.jsonAllocator);

            // generate a new UUID for the copied node to avoid conflicts if we attempt to duplicate the node
            Carrot::UUID newUUID{};
            copy["node_id"] = rapidjson::Value(newUUID.toString().c_str(), clipboard.jsonAllocator);

            EditorNode& instantiatedNode = loadSingleNode(copy);
            remap.emplaceBack(&instantiatedNode);

            const glm::vec2 offsetFromCenter = instantiatedNode.getPosition() - center;
            instantiatedNode.followMouseUntilClick(offsetFromCenter);
        }

        for (const auto& serializedLink : clipboard.links) {
            addLink(Link {
                .id = nextID(),
                .from = remap[serializedLink.nodeFrom]->getOutputs()[serializedLink.pinFrom],
                .to = remap[serializedLink.nodeTo]->getInputs()[serializedLink.pinTo],
            });
        }
    };

    auto issueCutCommand = [&]() {
        copyToClipboard(clipboard);

        for (auto& pLink : copiedLinks) {
            removeLink(*pLink);
        }
        for (auto& pNode : copiedNodes) {
            removeNode(*pNode);
        }
    };

    auto issueCopyCommand = [&]() {
        copyToClipboard(clipboard);
    };

    auto issuePasteCommand = [&]() {
        pasteFromClipboard(clipboard);
    };

    auto issueDuplicateCommand = [&]() {
        Clipboard localClipboard;
        copyToClipboard(localClipboard);
        pasteFromClipboard(localClipboard);
    };

    {


        if (cutQueued) {
            issueCutCommand();
        } else if (copyQueued) {
            issueCopyCommand();
        } else if (pasteQueued) {
            issuePasteCommand();
        } else if (duplicateQueued) {
            issueDuplicateCommand();
        }

        cutQueued = false;
        copyQueued = false;
        pasteQueued = false;
        duplicateQueued = false;
    }

    ed::End();

    isFocused = ImGui::IsWindowFocused();

    if(ImGui::BeginPopupContextWindow("##popup")) {
        if (ImGui::BeginMenu("Add node")) {
            recurseDrawNodeLibraryMenus(*rootMenu);

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Cut")) {
            issueCutCommand();
        }
        if (ImGui::MenuItem("Copy")) {
            issueCopyCommand();
        }
        if (ImGui::MenuItem("Paste")) {
            issuePasteCommand();
        }
        if (ImGui::MenuItem("Duplicate")) {
            issueDuplicateCommand();
        }
        ImGui::EndPopup();
    }

    ed::EnableShortcuts(false);

    for(const auto& tmpLabel : tmpLabels) {
        showLabel(tmpLabel.text);
    }
}

void Fertilizer::EditorGraph::tick(double deltaTime) {
    for(auto& l : tmpLabels) {
        l.remainingTime -= deltaTime;
    }

    tmpLabels.erase(std::find_if(tmpLabels.begin(), tmpLabels.end(), [&](const auto& l) { return l.remainingTime < 0.0f; }), tmpLabels.end());
}

void Fertilizer::EditorGraph::onCutShortcut() {
    if (!isFocused) {
        return;
    }
    cutQueued = true;
}

void Fertilizer::EditorGraph::onCopyShortcut() {
    if (!isFocused) {
        return;
    }
    copyQueued = true;
}

void Fertilizer::EditorGraph::onPasteShortcut() {
    if (!isFocused) {
        return;
    }
    pasteQueued = true;
}

void Fertilizer::EditorGraph::onDuplicateShortcut() {
    if (!isFocused) {
        return;
    }
    duplicateQueued = true;
}

void Fertilizer::EditorGraph::handleLinkCreation(std::shared_ptr<Fertilizer::Pin> pinA, std::shared_ptr<Fertilizer::Pin> pinB) {
    if(pinA->getType() != pinB->getType()) {
        auto inputPin = pinA->getType() == PinType::Input ? pinA : pinB;
        auto outputPin = pinA->getType() != PinType::Input ? pinA : pinB;
        auto linkPossibility = canLink(*outputPin, *inputPin);

        if(linkPossibility == LinkPossibility::Possible) {
            if(ed::AcceptNewItem(ImColor(128, 255, 128), 4.0f)) {
                addLink(Link {
                        .id = nextID(),
                        .from = outputPin,
                        .to = inputPin,
                });
            }
        } else {
            ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
            switch (linkPossibility) {
                case LinkPossibility::TooManyInputs:
                    showLabel("Too many inputs!");
                    break;
                case LinkPossibility::CyclicalGraph:
                    showLabel("Cyclical graph!");
                    break;
                case LinkPossibility::CannotLinkToSelf:
                    showLabel("Cannot link to self!");
                    break;
                case LinkPossibility::IncompatibleTypes:
                    showLabel("Incompatible types!");
                    break;
            }
        }
    } else {
        showLabel("Cannot link same pin type together!");
        ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
    }
}

void Fertilizer::EditorGraph::registerPin(std::shared_ptr<Fertilizer::Pin> pin) {
    id2pin[pin->id] = std::move(pin);
}

void Fertilizer::EditorGraph::addLink(const Fertilizer::Link& link) {
    links.push_back(link);
    markDirty();
}

void Fertilizer::EditorGraph::removeLink(const Link& link) {
    Carrot::removeIf(links, [&](const auto& l) { return l.id == link.id; });
    markDirty();
}

void Fertilizer::EditorGraph::addColoredLink(const Carrot::UUID& linkID) {
    coloredLinkUUIDs.insert(linkID);
}

void Fertilizer::EditorGraph::removeColoredLinks() {
    coloredLinkUUIDs.clear();
}

void Fertilizer::EditorGraph::unregisterPin(std::shared_ptr<Pin> pin) {
    if(pin->getType() == PinType::Input) {
        for(const auto& l : getLinksLeadingTo(*pin)) {
            removeLink(l);
        }
    } else {
        for(const auto& l : getLinksStartingFrom(*pin)) {
            removeLink(l);
        }
    }

    id2pin.erase(pin->id);
    id2uuid.erase(uuid2id[pin->id]);
    uuid2id.erase(pin->id);
}

void Fertilizer::EditorGraph::removeNode(const Fertilizer::EditorNode& node) {
    auto deletePins = [&](const auto& pins) {
        for (const auto& pin : pins) {
            unregisterPin(pin);
        }
    };
    deletePins(node.getInputs());
    deletePins(node.getOutputs());

    terminalNodes.erase(
            std::remove_if(terminalNodes.begin(), terminalNodes.end(), [&](const auto& n) {
                auto locked = n.lock();
                return !locked || locked->getID() == node.getID();
            }),
            terminalNodes.end());

    id2node.erase(node.getID());
    id2uuid.erase(uuid2id[node.getID()]);
    uuid2id.erase(node.getID());

    markDirty();
}

Fertilizer::LinkPossibility Fertilizer::EditorGraph::canLink(Fertilizer::Pin& from, Fertilizer::Pin& to) {
    if(from.owner.getID() == to.owner.getID())
        return LinkPossibility::CannotLinkToSelf;
    if(from.getExpressionType() != to.getExpressionType()) {
        return LinkPossibility::IncompatibleTypes;
    }
    for(const auto& link : links) {
        if(auto linkTo = link.to.lock()) {
            if(linkTo->id == to.id) {
                return LinkPossibility::TooManyInputs;
            }
        }
    }
    // check cycle
    std::queue<Carrot::UUID> queued;
    queued.push(from.owner.getID());
    while(!queued.empty()) {
        auto nodeID = queued.back();
        queued.pop();

        auto& node = id2node[nodeID];

        for(const auto& input : node->getInputs()) {
            auto linkLeadingToInput = getLinksLeadingTo(*input);
            for(const auto& link : linkLeadingToInput) {
                if(auto locked = link.from.lock()) {
                    if(locked->owner.getID() == to.owner.getID())
                        return LinkPossibility::CyclicalGraph;

                    queued.push(locked->owner.getID());
                }
            }
        }
    }
    return LinkPossibility::Possible;
}

bool Fertilizer::EditorGraph::hasLinksLeadingTo(const Fertilizer::Pin& to) const {
    for(const auto& link : links) {
        if(auto linkTo = link.to.lock()) {
            if(linkTo->id == to.id) {
                return true;
            }
        }
    }
    return false;
}

bool Fertilizer::EditorGraph::hasLinksStartingFrom(const Fertilizer::Pin& from) const {
    for(const auto& link : links) {
        if(auto linkTo = link.from.lock()) {
            if(linkTo->id == from.id) {
                return true;
            }
        }
    }
    return false;
}

std::vector<Fertilizer::Link> Fertilizer::EditorGraph::getLinksLeadingTo(const Fertilizer::Pin& to) const {
    std::vector<Fertilizer::Link> result;
    for(const auto& link : links) {
        if(auto linkTo = link.to.lock()) {
            if(linkTo->id == to.id) {
                result.push_back(link);
            }
        }
    }
    return result;
}

std::vector<Fertilizer::Link> Fertilizer::EditorGraph::getLinksStartingFrom(const Fertilizer::Pin& from) const {
    std::vector<Fertilizer::Link> result;
    for(const auto& link : links) {
        if(auto linkFrom = link.from.lock()) {
            if(linkFrom->id == from.id) {
                result.push_back(link);
            }
        }
    }
    return result;
}

Fertilizer::EditorGraph::~EditorGraph() {
    clear();
    ed::DestroyEditor(g_Context);
    g_Context = nullptr;
}

Fertilizer::EditorNode& Fertilizer::EditorGraph::loadSingleNode(const rapidjson::Value& nodeJSON) {
    std::string internalName = nodeJSON["node_type"].GetString();
    auto& initialiser = nodeLibrary[internalName];
    if(initialiser) {
        return (*initialiser)(*this, nodeJSON);
    } else {
        throw ParseError("Unknown internalName: " + internalName);
    }
}

void Fertilizer::EditorGraph::loadFromJSON(const rapidjson::Value& json) {
    clear();
    auto nodeArray = json["nodes"].GetArray();
    for(const auto& nodeJSON : nodeArray) {
        loadSingleNode(nodeJSON);
    }

    auto getPin = [&](const rapidjson::Value& json) -> std::shared_ptr<Pin> {
        auto node = id2node[Carrot::UUID::fromString(json["node_id"].GetString())];
        if(!node) {
            throw ParseError("Invalid node ID: " + std::string(json["node_id"].GetString()));
        }

        uint32_t pinIndex = json["pin_index"].GetInt64();

        PinType type = std::string(json["pin_type"].GetString()) == "input" ? PinType::Input : PinType::Output;
        switch(type) {
            case PinType::Input:
                return node->getInputs()[pinIndex];
            case PinType::Output:
                return node->getOutputs()[pinIndex];
        }
        return nullptr;
    };
    auto linkArray = json["links"].GetArray();
    for(const auto& linkJSON : linkArray) {
        auto from = getPin(linkJSON["from"]);
        auto to = getPin(linkJSON["to"]);
        if(from && to) {
            links.push_back(Link {
                    .id = nextID(),
                    .from = from,
                    .to = to,
            });
        }
    }

    zoomToContent = true;
    hasUnsavedChanges = false;
}

uint32_t Fertilizer::EditorGraph::nextFreeEditorID() {
    bool alreadyUsed;
    uint32_t result;
    do {
        result = uniqueID++;
        alreadyUsed = id2uuid.find(result) != id2uuid.end();
    } while(alreadyUsed);
    return result;
}

rapidjson::Value Fertilizer::EditorGraph::toJSON(rapidjson::Document& document) {
    rapidjson::Value result;
    result.SetObject();

    auto nodeArray = rapidjson::Value(rapidjson::kArrayType);
    for(const auto& [id, node] : id2node) {
        nodeArray.PushBack(node->toJSON(document.GetAllocator()), document.GetAllocator());
    }
    result.AddMember("nodes", nodeArray, document.GetAllocator());

    auto linkArray = rapidjson::Value(rapidjson::kArrayType);
    for(const auto& link : links) {
        if(auto to = link.to.lock()) {
            if (auto from = link.from.lock()) {
                linkArray.PushBack(std::move(rapidjson::Value(rapidjson::kObjectType)
                                                     .AddMember("from", std::move(from->toJSONReference(document)),
                                                                document.GetAllocator())
                                                     .AddMember("to", std::move(to->toJSONReference(document)),
                                                                document.GetAllocator())
                ), document.GetAllocator());
            }
        }
    }
    result.AddMember("links", linkArray, document.GetAllocator());

    return result;
}

void Fertilizer::EditorGraph::clear() {
    id2node.clear();
    id2pin.clear();
    id2uuid.clear();
    uuid2id.clear();
    terminalNodes.clear();
    uniqueID = 1;
    hasUnsavedChanges = false;
}

Carrot::UUID Fertilizer::EditorGraph::nextID() {
    Carrot::UUID uuid;
    while(uuid2id.find(uuid) != uuid2id.end()) {
        uuid = Carrot::UUID();
    }
    reserveID(uuid);
    return uuid;
}

uint32_t Fertilizer::EditorGraph::reserveID(const Carrot::UUID& uuid) {
    uint32_t id = nextFreeEditorID();
    id2uuid[id] = uuid;
    uuid2id[uuid] = id;
    return id;
}

uint32_t Fertilizer::EditorGraph::getEditorID(const Carrot::UUID& uuid) {
    return uuid2id[uuid];
}

void Fertilizer::EditorGraph::showLabel(const std::string& text, ImColor color) {
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeight());
    auto size = ImGui::CalcTextSize(text.c_str());

    auto padding = ImGui::GetStyle().FramePadding;
    auto spacing = ImGui::GetStyle().ItemSpacing;

    ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(spacing.x, -spacing.y));

    auto rectMin = ImGui::GetCursorScreenPos() - padding;
    auto rectMax = ImGui::GetCursorScreenPos() + size + padding;

    auto drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(rectMin, rectMax, color, size.y * 0.15f);
    ImGui::TextUnformatted(text.c_str());
}

void Fertilizer::EditorGraph::addTemporaryLabel(const std::string& text) {
    tmpLabels.emplace_back(std::move(TemporaryLabel(text)));
}

std::vector<std::shared_ptr<Carrot::Expression>> Fertilizer::EditorGraph::generateExpressionsFromTerminalNodes(std::unordered_set<Carrot::UUID>& activeLinks) const {
    std::vector<std::shared_ptr<Carrot::Expression>> result;

    for(const auto& terminalNode : terminalNodes) {
        if(auto node = terminalNode.lock()) {
            result.push_back(node->toExpression(0, activeLinks));
        }
    }

    return std::move(result);
}

void Fertilizer::EditorGraph::markDirty() {
    hasUnsavedChanges = true;
    lastChangeTime = std::chrono::steady_clock::now();
}

std::chrono::time_point<std::chrono::steady_clock> Fertilizer::EditorGraph::getLastChangeTime() const {
    return lastChangeTime;
}

void Fertilizer::EditorGraph::resetChangeFlag() {
    hasUnsavedChanges = false;
}

bool Fertilizer::EditorGraph::hasChanges() const {
    return hasUnsavedChanges;
}

void Fertilizer::EditorGraph::addTemplatesToLibrary() {
    namespace fs = std::filesystem;

    struct TemplateInit: public NodeInitialiserBase {
        explicit TemplateInit(std::string id): id(std::move(id)) {}

        std::string id;

        inline EditorNode& operator()(EditorGraph& graph, const rapidjson::Value& json) override {
            throw std::runtime_error("SHOULD NOT BE CALLED");
        };

        EditorNode& operator()(EditorGraph& graph) override {
            return graph.newNode<TemplateNode>(id);
        };

        NodeValidity getValidity(EditorGraph& graph) const override {
            return NodeValidity::Possible;
        };
    };

    auto previousMenu = currentMenu;
    for(const auto& folder : TemplateNode::Paths) {
        if(!fs::exists(folder))
            continue;

        for(const auto& file : fs::directory_iterator(folder)) {
            auto path = fs::path(file);
            if(path.has_extension() && path.extension() == ".json") {
                auto templateName = path.stem();

                rapidjson::Document description;
                description.Parse(Carrot::IO::readFileAsText(path.string()).c_str());

                std::string title = description["title"].GetString();
                std::string menuLocation = description["menu_location"].GetString();

                NodeLibraryMenu* menu = rootMenu.get();
                for(const auto& part : Carrot::splitString(menuLocation, "/")) {
                    bool found = false;
                    for(auto& submenu : menu->getSubmenus()) {
                        if(submenu.getName() == part) {
                            menu = &submenu;
                            found = true;
                            break;
                        }
                    }

                    if(!found) {
                        menu = &menu->newChild(part);
                    }
                }
                currentMenu = menu;
                addToLibrary("template_internal_name_dont_save_"+templateName.string(), title, std::make_unique<TemplateInit>(templateName.string()));
            }
        }
    }
    currentMenu = previousMenu;
}

void Fertilizer::EditorGraph::recurseDrawNodeLibraryMenus(const NodeLibraryMenu& menu) {
    for(const auto& internalName : menu.getEntries()) {
        auto& nodeCreator = nodeLibrary[internalName];
        const auto& title = internalName2title[internalName];
        auto& init = (*nodeCreator);
        if(ImGui::MenuItem(title.c_str())) {
            NodeValidity validity = init.getValidity(*this);
            switch(validity) {
                case NodeValidity::Possible: {
                    auto& node = init(*this);
                    node.setPosition(ImVec2(0,0));
                    node.followMouseUntilClick({}); // makes it easier to position the node after creating it
                    markDirty();
                } break;

                case NodeValidity::TerminalOfSameTypeAlreadyExists:
                    addTemporaryLabel("Terminal node of same type already exists!");
                    break;
            }
        }
    }

    for(const auto& submenu : menu.getSubmenus()) {
        if(ImGui::BeginMenu(submenu.getName().c_str())) {
            recurseDrawNodeLibraryMenus(submenu);

            ImGui::EndMenu();
        }
    }

}
