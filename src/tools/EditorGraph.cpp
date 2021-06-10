//
// Created by jglrxavpok on 02/05/2021.
//

#include "EditorGraph.h"

#include <utility>
#include <queue>
#include "nodes/Arithmetics.hpp"
#include "nodes/Constants.hpp"
#include <engine/utils/ImGuiUtils.hpp>
#include <engine/utils/Containers.h>
#include <filesystem>
#include <iostream>
#include <engine/utils/stringmanip.h>

namespace ed = ax::NodeEditor;

Tools::EditorGraph::EditorGraph(Carrot::Engine& engine, std::string name): engine(engine), name(std::move(name)) {
    config.NavigateButtonIndex = 2;
    g_Context = ed::CreateEditor(&config);

    ed::SetCurrentEditor(g_Context);

    rootMenu = std::make_unique<NodeLibraryMenu>("<root>");
    currentMenu = rootMenu.get();

    imguiTextures.imageStorage.expressionTypes[Carrot::ExpressionTypes::Bool] = Carrot::Image::fromFile(engine.getVulkanDriver(), "resources/textures/icons/boolean.png");
    imguiTextures.imageStorage.expressionTypes[Carrot::ExpressionTypes::Float] = Carrot::Image::fromFile(engine.getVulkanDriver(), "resources/textures/icons/float.png");

    auto store = [&](const Carrot::ExpressionType& type)
    {
        imguiTextures.imageViewStorage.expressionTypes[type] = std::move(imguiTextures.imageStorage.expressionTypes[type]->createImageView());
    };

    auto addTexture = [&](const Carrot::ExpressionType& type)
    {
        store(type);
        ImTextureID id = ImGui_ImplVulkan_AddTexture(engine.getVulkanDriver().getNearestSampler(),
                                            *imguiTextures.imageViewStorage.expressionTypes[type],
                                                    static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));

        imguiTextures.expressionTypes[type] = id;
    };

    addTexture(Carrot::ExpressionTypes::Bool);
    addTexture(Carrot::ExpressionTypes::Float);
}

void Tools::EditorGraph::onFrame(size_t frameIndex) {
    ed::SetCurrentEditor(g_Context);
    ed::EnableShortcuts(true);

    ed::Begin(name.c_str());

    for(auto& [id, node] : id2node) {
        node->draw();
    }

    for(const auto& link : links) {
        if(auto input = link.to.lock()) {
            if(auto output = link.from.lock()) {
                ed::Link(getEditorID(link.id), getEditorID(input->id), getEditorID(output->id));
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

    ed::End();

    if(ImGui::BeginPopupContextWindow("##popup")) {
        if (ImGui::BeginMenu("Add node")) {
            recurseDrawNodeLibraryMenus(*rootMenu);

            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    ed::EnableShortcuts(false);

    for(const auto& tmpLabel : tmpLabels) {
        showLabel(tmpLabel.text);
    }
}

void Tools::EditorGraph::tick(double deltaTime) {
    for(auto& l : tmpLabels) {
        l.remainingTime -= deltaTime;
    }

    tmpLabels.erase(std::find_if(tmpLabels.begin(), tmpLabels.end(), [&](const auto& l) { return l.remainingTime < 0.0f; }), tmpLabels.end());
}

void Tools::EditorGraph::handleLinkCreation(std::shared_ptr<Tools::Pin> pinA, std::shared_ptr<Tools::Pin> pinB) {
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

void Tools::EditorGraph::registerPin(std::shared_ptr<Tools::Pin> pin) {
    id2pin[pin->id] = std::move(pin);
}

void Tools::EditorGraph::addLink(Tools::Link link) {
    links.push_back(link);
    hasUnsavedChanges = true;
}

void Tools::EditorGraph::removeLink(const Link& link) {
    Carrot::removeIf(links, [&](const auto& l) { return l.id == link.id; });
    hasUnsavedChanges = true;
}

void Tools::EditorGraph::unregisterPin(shared_ptr<Pin> pin) {
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

void Tools::EditorGraph::removeNode(const Tools::EditorNode& node) {
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

    hasUnsavedChanges = true;
}

Tools::LinkPossibility Tools::EditorGraph::canLink(Tools::Pin& from, Tools::Pin& to) {
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

std::vector<Tools::Link> Tools::EditorGraph::getLinksLeadingTo(const Tools::Pin& to) const {
    std::vector<Tools::Link> result;
    for(const auto& link : links) {
        if(auto linkTo = link.to.lock()) {
            if(linkTo->id == to.id) {
                result.push_back(link);
            }
        }
    }
    return result;
}

std::vector<Tools::Link> Tools::EditorGraph::getLinksStartingFrom(const Tools::Pin& from) const {
    std::vector<Tools::Link> result;
    for(const auto& link : links) {
        if(auto linkFrom = link.from.lock()) {
            if(linkFrom->id == from.id) {
                result.push_back(link);
            }
        }
    }
    return result;
}

Tools::EditorGraph::~EditorGraph() {
    clear();
    ed::DestroyEditor(g_Context);
    g_Context = nullptr;
}

void Tools::EditorGraph::loadFromJSON(const rapidjson::Value& json) {
    clear();
    auto nodeArray = json["nodes"].GetArray();
    for(const auto& nodeJSON : nodeArray) {
        std::string internalName = nodeJSON["node_type"].GetString();
        auto& initialiser = nodeLibrary[internalName];
        if(initialiser) {
            (*initialiser)(*this, nodeJSON);
        } else {
            throw ParseError("Unknown internalName: " + internalName);
        }
    }

    auto getPin = [&](const rapidjson::Value& json) -> std::shared_ptr<Pin> {
        auto node = id2node[Carrot::fromString(json["node_id"].GetString())];
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

uint32_t Tools::EditorGraph::nextFreeEditorID() {
    bool alreadyUsed;
    uint32_t result;
    do {
        result = uniqueID++;
        alreadyUsed = id2uuid.find(result) != id2uuid.end();
    } while(alreadyUsed);
    return result;
}

rapidjson::Value Tools::EditorGraph::toJSON(rapidjson::Document& document) {
    rapidjson::Value result;
    result.SetObject();

    auto& nodeArray = rapidjson::Value().SetArray();
    for(const auto& [id, node] : id2node) {
        nodeArray.PushBack(node->toJSON(document), document.GetAllocator());
    }
    result.AddMember("nodes", nodeArray, document.GetAllocator());

    auto& linkArray = rapidjson::Value().SetArray();
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

void Tools::EditorGraph::clear() {
    id2node.clear();
    id2pin.clear();
    id2uuid.clear();
    uuid2id.clear();
    terminalNodes.clear();
    uniqueID = 1;
    hasUnsavedChanges = false;
}

Carrot::UUID Tools::EditorGraph::nextID() {
    Carrot::UUID uuid = Carrot::randomUUID();
    while(uuid2id.find(uuid) != uuid2id.end()) {
        uuid = Carrot::randomUUID();
    }
    reserveID(uuid);
    return uuid;
}

uint32_t Tools::EditorGraph::reserveID(const Carrot::UUID& uuid) {
    uint32_t id = nextFreeEditorID();
    id2uuid[id] = uuid;
    uuid2id[uuid] = id;
    return id;
}

uint32_t Tools::EditorGraph::getEditorID(const Carrot::UUID& uuid) {
    return uuid2id[uuid];
}

void Tools::EditorGraph::showLabel(const std::string& text, ImColor color) {
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

void Tools::EditorGraph::addTemporaryLabel(const string& text) {
    tmpLabels.emplace_back(std::move(TemporaryLabel(text)));
}

std::vector<std::shared_ptr<Carrot::Expression>> Tools::EditorGraph::generateExpressionsFromTerminalNodes() const {
    std::vector<std::shared_ptr<Carrot::Expression>> result;

    for(const auto& terminalNode : terminalNodes) {
        if(auto node = terminalNode.lock()) {
            result.push_back(node->toExpression(0));
        }
    }

    return std::move(result);
}

void Tools::EditorGraph::resetChangeFlag() {
    hasUnsavedChanges = false;
}

bool Tools::EditorGraph::hasChanges() const {
    return hasUnsavedChanges;
}

void Tools::EditorGraph::addTemplatesToLibrary() {
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
                description.Parse(IO::readFileAsText(path.string()).c_str());

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

void Tools::EditorGraph::recurseDrawNodeLibraryMenus(const NodeLibraryMenu& menu) {
    for(const auto& internalName : menu.getEntries()) {
        auto& nodeCreator = nodeLibrary[internalName];
        auto title = internalName2title[internalName];
        auto& init = (*nodeCreator);
        if(ImGui::MenuItem(title.c_str())) {
            auto validity = init.getValidity(*this);
            switch(validity) {
                case NodeValidity::Possible:
                    init(*this).setPosition(ed::ScreenToCanvas(ImGui::GetMousePos()));
                    break;

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
