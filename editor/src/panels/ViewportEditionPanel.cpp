//
// Created by jglrxavpok on 05/05/2026.
//

#include "../Peeler.h"
#include <panels/ViewportEditionPanel.h>
#include <core/utils/ImGuiUtils.hpp>

namespace Peeler {

    class ChangeCurrentComposition final : public ICommand {
    public:
        ChangeCurrentComposition(Application& app, const std::string& compositionID)
            : ICommand(app, Carrot::sprintf("Changed compositionID to %s", compositionID.c_str())), compositionID(compositionID) {
            previousCompositionID = editor.getViewportEditionPanel().currentCompositionID;
        }

        void undo() override {
            ViewportEditionPanel& viewportPanel = editor.getViewportEditionPanel();
            viewportPanel.currentCompositionID = previousCompositionID;
            viewportPanel.compositionRefresh = viewportPanel.getSelectedCompositionIndex();
        }

        void redo() override {
            ViewportEditionPanel& viewportPanel = editor.getViewportEditionPanel();
            viewportPanel.currentCompositionID = compositionID;
            viewportPanel.compositionRefresh = viewportPanel.getSelectedCompositionIndex();
        }

    private:
        std::string previousCompositionID;
        std::string compositionID;
    };

    class ChangeViewportSelection final : public ICommand {
    public:
        ChangeViewportSelection(Application& app, const Carrot::Identifier& viewportID)
            : ICommand(app, Carrot::sprintf("Changed selectedViewport to %s", std::string{viewportID}.c_str())), viewportID(viewportID) {
            ViewportEditionPanel& viewportPanel = editor.getViewportEditionPanel();
            previousViewportID = viewportPanel.composition2selectedViewport[viewportPanel.currentCompositionID];
        }

        void undo() override {
            ViewportEditionPanel& viewportPanel = editor.getViewportEditionPanel();
            viewportPanel.composition2selectedViewport[viewportPanel.currentCompositionID] = previousViewportID;
        }

        void redo() override {
            ViewportEditionPanel& viewportPanel = editor.getViewportEditionPanel();
            viewportPanel.composition2selectedViewport[viewportPanel.currentCompositionID] = viewportID;
        }

    private:
        Carrot::Identifier previousViewportID;
        Carrot::Identifier viewportID;
    };

    class CreateNewComposition final : public ICommand {
    public:
        explicit CreateNewComposition(Application& app): ICommand(app, "Create new composition") {
            ViewportEditionPanel& panel = editor.getViewportEditionPanel();
            index = panel.compositions.size();
            name = Carrot::sprintf("New composition %d", index);
        }

        void undo() override {
            ViewportEditionPanel& panel = editor.getViewportEditionPanel();
            panel.compositions.remove(index);
        }

        void redo() override {
            ViewportEditionPanel& panel = editor.getViewportEditionPanel();
            verify(index == panel.compositions.size(), "should always be added at the end");
            panel.compositions.emplaceBack(name, Carrot::Render::ViewportComposition{});
        }

        const std::string& getCreatedName() const {
            return name;
        }

    private:
        std::string name;
        i64 index = 0;
    };

    class ChangeCompositionName final : public ICommand {
    public:
        explicit ChangeCompositionName(Application& app, i32 index, std::string newName): ICommand(app, "Change composition name"), index(index), name(std::move(newName)) {
            ViewportEditionPanel& panel = editor.getViewportEditionPanel();
            oldName = panel.compositions[index].first;
        }

        void undo() override {
            ViewportEditionPanel& panel = editor.getViewportEditionPanel();
            panel.compositions[index].first = oldName;
        }

        void redo() override {
            ViewportEditionPanel& panel = editor.getViewportEditionPanel();
            panel.compositions[index].first = name;
        }

    private:
        std::string oldName;
        std::string name;
        i64 index = 0;
    };

    class CreateNewViewport final : public ICommand {
    public:
        explicit CreateNewViewport(Application& app): ICommand(app, "Create new viewport") {
            ViewportEditionPanel& panel = editor.getViewportEditionPanel();
            index = panel.getSelectedCompositionIndex();

            i32 viewportIndex = panel.compositions[index].second.viewports.size();
            name = Carrot::Identifier{Carrot::sprintf("New viewport %d", viewportIndex)};
        }

        void undo() override {
            ViewportEditionPanel& panel = editor.getViewportEditionPanel();
            panel.compositions[index].second.viewports.erase(name);
            panel.compositionRefresh = index;
        }

        void redo() override {
            ViewportEditionPanel& panel = editor.getViewportEditionPanel();
            panel.compositions[index].second.viewports[name] = {};
            panel.compositionRefresh = index;
        }

        const Carrot::Identifier& getCreatedName() const {
            return name;
        }

    private:
        Carrot::Identifier name;
        i64 index = 0;
    };

    template<typename TElement>
    class ModifyViewportCommand final : public ICommand {
    public:
        ModifyViewportCommand(Application& app, TElement& ref, TElement newValue): ICommand(app, "Modify viewport"), ref(ref), newValue(newValue) {
            oldValue = ref;
        }

        void undo() override {
            ref = oldValue;
            ViewportEditionPanel& panel = editor.getViewportEditionPanel();
            i64 index = panel.getSelectedCompositionIndex();
            panel.compositionRefresh = index;
        }

        void redo() override {
            ref = newValue;
            ViewportEditionPanel& panel = editor.getViewportEditionPanel();
            i64 index = panel.getSelectedCompositionIndex();
            panel.compositionRefresh = index;
        }

    private:
        TElement& ref;
        TElement newValue;
        TElement oldValue;
    };

    class RenameViewportCommand final : public ICommand {
    public:
        RenameViewportCommand(Application& app, const Carrot::Identifier& from, const Carrot::Identifier& to): ICommand(app, "Rename viewport"), from(from), to(to) {}

        void undo() override {
            ViewportEditionPanel& viewportPanel = editor.getViewportEditionPanel();
            Carrot::Render::ViewportComposition& composition = viewportPanel.compositions[viewportPanel.getSelectedCompositionIndex()].second;
            auto node = composition.viewports.extract(to);
            node.key() = from;
            composition.viewports.insert(std::move(node));
        }

        void redo() override {
            ViewportEditionPanel& viewportPanel = editor.getViewportEditionPanel();
            Carrot::Render::ViewportComposition& composition = viewportPanel.compositions[viewportPanel.getSelectedCompositionIndex()].second;
            auto node = composition.viewports.extract(from);
            node.key() = to;
            composition.viewports.insert(std::move(node));
        }

    private:
        Carrot::Identifier from;
        Carrot::Identifier to;
    };

    class ApplyCompositionPresetCommand final : public ICommand {
    public:
        ApplyCompositionPresetCommand(Application& editor, Carrot::Render::ViewportComposition&& composition): ICommand(editor, "Apply composition preset"), preset(std::move(composition)) {
            ViewportEditionPanel& panel = editor.getViewportEditionPanel();
            saved.copyViewportPositions(panel.compositions[panel.getSelectedCompositionIndex()].second);
        }

        void undo() override {
            ViewportEditionPanel& panel = editor.getViewportEditionPanel();
            panel.compositions[panel.getSelectedCompositionIndex()].second.copyViewportPositions(saved);
            i64 index = panel.getSelectedCompositionIndex();
            panel.compositionRefresh = index;
        }

        void redo() override {
            ViewportEditionPanel& panel = editor.getViewportEditionPanel();
            panel.compositions[panel.getSelectedCompositionIndex()].second.copyViewportPositions(preset);
            i64 index = panel.getSelectedCompositionIndex();
            panel.compositionRefresh = index;
        }

    private:
        Carrot::Render::ViewportComposition saved;
        Carrot::Render::ViewportComposition preset;
    };

    ViewportEditionPanel::ViewportEditionPanel(Peeler::Application& editor): EditorPanel(editor) {

    }

    void ViewportEditionPanel::loadCompositionsFromVFS() {
        compositions.clear();
        for (const auto entry : GetVFS().iterateOverDirectory(Carrot::IO::VFS::Path{"game", "compositions"})) {
            Carrot::Render::ViewportComposition composition;
            composition.deserialize(entry);
            compositions.emplaceBack(Carrot::Pair{std::string{entry.getPath().getStem()}, std::move(composition)});
        }
    }

    void ViewportEditionPanel::draw(const Carrot::Render::Context& renderContext) {
        if (showViewportSettings) {
            i32 selectedCompositionIndex = -1;
            if (ImGui::Begin("Viewport settings", &showViewportSettings)) {
                const ImVec2 availableSize = ImGui::GetContentRegionAvail();
                const ImVec2 listSize = ImVec2(availableSize.x * 0.3f, availableSize.y * 0.5f);
                if (ImGui::BeginChild("##list", listSize, ImGuiChildFlags_Borders)) {
                    ImGui::SeparatorText("Compositions");
                    i32 index = 0;
                    for (auto& [compositionID, composition] : compositions) {
                        const bool isSelected = compositionID == currentCompositionID;
                        if (isSelected) {
                            selectedCompositionIndex = index;
                        }
                        if (ImGui::Selectable(compositionID.c_str(), isSelected)) {
                            app.undoStack.push<ChangeCurrentComposition>(compositionID);
                        }
                        index++;
                    }

                    if (ImGui::Button("+")) {
                        auto create = Carrot::makeUnique<CreateNewComposition>(Carrot::Allocator::getDefault(), app);
                        auto changeSelection = Carrot::makeUnique<ChangeCurrentComposition>(Carrot::Allocator::getDefault(), app, create->getCreatedName());
                        app.undoStack.push<CompoundCommand>(std::move(create), std::move(changeSelection));
                    }
                }
                ImGui::EndChild();

                ImGui::SameLine();
                if (ImGui::BeginChild("##edition", ImVec2(availableSize.x - listSize.x, listSize.y), ImGuiChildFlags_Borders)) {
                    if (selectedCompositionIndex >= 0 && selectedCompositionIndex < compositions.size()) {
                        std::string nameCopy = compositions[selectedCompositionIndex].first;
                        Carrot::Render::ViewportComposition& composition = compositions[selectedCompositionIndex].second;

                        if (ImGui::InputText("Composition name", nameCopy)) {
                            auto rename = Carrot::makeUnique<ChangeCompositionName>(Carrot::Allocator::getDefault(), app, selectedCompositionIndex, nameCopy);
                            auto changeSelection = Carrot::makeUnique<ChangeCurrentComposition>(Carrot::Allocator::getDefault(), app, nameCopy);
                            app.undoStack.push<CompoundCommand>(std::move(rename), std::move(changeSelection));
                        }

                        const ImVec2 viewportEditionSize = ImGui::GetContentRegionAvail();
                        const ImVec2 viewportListSize = ImVec2(viewportEditionSize.x * 0.2f, viewportEditionSize.y);
                        const Carrot::Identifier& selectedViewportID = composition2selectedViewport[nameCopy];
                        // TODO sort by name?
                        if (ImGui::BeginChild("##list", viewportListSize)) {
                            ImGui::SeparatorText("Viewports");
                            for (auto& [viewportID, loc] : composition.viewports) {
                                const std::string idAsString {viewportID};
                                const bool isSelected = selectedViewportID == viewportID;
                                if (ImGui::Selectable(idAsString.c_str(), isSelected)) {
                                    app.undoStack.push<ChangeViewportSelection>(viewportID);
                                }
                            }

                            if (ImGui::Button("+")) {
                                auto create = Carrot::makeUnique<CreateNewViewport>(Carrot::Allocator::getDefault(), app);
                                auto changeSelection = Carrot::makeUnique<ChangeViewportSelection>(Carrot::Allocator::getDefault(), app, create->getCreatedName());
                                app.undoStack.push<CompoundCommand>(std::move(create), std::move(changeSelection));
                            }
                            if (ImGui::BeginMenu("Apply preset")) {
                                if (ImGui::MenuItem("Single screen")) {
                                    app.undoStack.push<ApplyCompositionPresetCommand>(std::move(Carrot::Render::ViewportComposition{}));
                                }

                                if (ImGui::MenuItem("Split screen (2 players)")) {
                                    Carrot::Render::ViewportComposition splitScreen2Players;
                                    Carrot::Render::ViewportLocation& player0Loc = splitScreen2Players.viewports[Carrot::Identifier{"player_0"}];
                                    player0Loc.offset = {0, 0};
                                    player0Loc.size = {1.0f, 0.5f};

                                    Carrot::Render::ViewportLocation& player1Loc = splitScreen2Players.viewports[Carrot::Identifier{"player_1"}];
                                    player1Loc.offset = {0, 0.5f};
                                    player1Loc.size = {1.0f, 0.5f};
                                    app.undoStack.push<ApplyCompositionPresetCommand>(std::move(splitScreen2Players));
                                }

                                if (ImGui::MenuItem("Split screen (4 players)")) {
                                    Carrot::Render::ViewportComposition splitScreen4Players;
                                    Carrot::Render::ViewportLocation& player0Loc = splitScreen4Players.viewports[Carrot::Identifier{"player_0"}];
                                    player0Loc.offset = {0, 0};
                                    player0Loc.size = {0.5f, 0.5f};

                                    Carrot::Render::ViewportLocation& player1Loc = splitScreen4Players.viewports[Carrot::Identifier{"player_1"}];
                                    player1Loc.offset = {0.5f, 0.0f};
                                    player1Loc.size = {0.5f, 0.5f};

                                    Carrot::Render::ViewportLocation& player2Loc = splitScreen4Players.viewports[Carrot::Identifier{"player_2"}];
                                    player2Loc.offset = {0.0f, 0.5f};
                                    player2Loc.size = {0.5f, 0.5f};

                                    Carrot::Render::ViewportLocation& player3Loc = splitScreen4Players.viewports[Carrot::Identifier{"player_3"}];
                                    player3Loc.offset = {0.5f, 0.5f};
                                    player3Loc.size = {0.5f, 0.5f};
                                    app.undoStack.push<ApplyCompositionPresetCommand>(std::move(splitScreen4Players));
                                }
                                ImGui::EndMenu();
                            }
                        }
                        ImGui::EndChild();
                        ImGui::SameLine();
                        if (ImGui::BeginChild("##viewport pos", ImVec2(viewportEditionSize.x - viewportListSize.x, viewportListSize.y))) {
                            Carrot::Render::ViewportLocation* pLocation = nullptr;
                            auto iter = composition.viewports.find(selectedViewportID);
                            if (iter != composition.viewports.end()) {
                                pLocation = &iter->second;
                            }
                            if (pLocation) {
                                float offset[2] = {pLocation->offset.x, pLocation->offset.y};
                                float size[2] = {pLocation->size.x, pLocation->size.y};
                                float z = pLocation->z;

                                std::string viewportName {selectedViewportID};
                                if (ImGui::InputText("Viewport name", viewportName)) {
                                    Carrot::Identifier to{viewportName};
                                    auto renameCommand = Carrot::makeUnique<RenameViewportCommand>(Carrot::Allocator::getDefault(), app, selectedViewportID, to);
                                    auto changeSelection = Carrot::makeUnique<ChangeViewportSelection>(Carrot::Allocator::getDefault(), app, to);
                                    app.undoStack.push<CompoundCommand>(std::move(renameCommand), std::move(changeSelection));
                                }
                                if (ImGui::InputFloat2("Offset", offset)) {
                                    app.undoStack.push<ModifyViewportCommand<glm::vec2>>(pLocation->offset, glm::vec2{offset[0], offset[1]});
                                }
                                if (ImGui::InputFloat2("Size", size)) {
                                    app.undoStack.push<ModifyViewportCommand<glm::vec2>>(pLocation->size, glm::vec2{size[0], size[1]});
                                }
                                if (ImGui::InputFloat("Z", &z)) {
                                    app.undoStack.push<ModifyViewportCommand<float>>(pLocation->z, z);
                                }
                            }
                        }
                        ImGui::EndChild();
                    }
                }
                ImGui::EndChild();

                ImGui::Separator();
                // preview
                ImVec2 previewSize = ImVec2(availableSize.y*0.5f * 16.0f/9.0f, availableSize.y*0.5f);
                ImDrawList* pDrawList = ImGui::GetWindowDrawList();
                {
                    ImVec2 startPos = ImGui::GetCursorScreenPos();
                    startPos.x += availableSize.x*0.5f - previewSize.x / 2.0f;

                    const float fontSize = 23.0f;
                    ImGui::PushFont(ImGui::GetFont(), fontSize);
                    CLEANUP(ImGui::PopFont());
                    if (selectedCompositionIndex >= 0 && selectedCompositionIndex < compositions.size()) {
                        Carrot::Render::ViewportComposition& composition = compositions[selectedCompositionIndex].second;
                        // TODO: sort by Z
                        for (const auto& [id, location] : composition.viewports) {
                            const ImVec2 start = startPos + ImVec2(previewSize.x * location.offset.x, previewSize.y * location.offset.y);
                            const ImVec2 size(previewSize.x * location.size.x, previewSize.y * location.size.y);
                            const ImVec2 end = start + size;
                            ImColor color = ImGuiUtils::getDebugColorFromPaletteAsImColor(id.getHash());
                            color.Value.w = 0.5f;
                            pDrawList->AddRectFilled(start, end, color);
                            const std::string text {id};

                            const ImVec2 textSize = ImGui::CalcTextSize(text.c_str(), text.c_str() + text.size());

                            ImVec2 textPosition = start + size / 2 - textSize / 2; // center text

                            // shadow under text
                            pDrawList->AddText(ImGui::GetFont(), fontSize, textPosition + ImVec2(1, 1), ImColor(0.0f, 0.0f, 0.0f, 1.0f), text.c_str(), text.c_str()+text.size(), size.x);
                            pDrawList->AddText(ImGui::GetFont(), fontSize, textPosition, ImColor(1.0f, 1.0f, 1.0f, 1.0f), text.c_str(), text.c_str()+text.size(), size.x);
                        }
                    }
                }
                ImGui::Dummy(previewSize);
            }
            ImGui::End();

            if (compositionRefresh.has_value()) {
                Carrot::Render::ViewportComposition copy;
                copy.copyViewportPositions(compositions[compositionRefresh.value()].second);
                app.gameTexture = app.updateViewportComposition(std::move(copy));
                compositionRefresh.reset();
            }
        }
    }

    i32 ViewportEditionPanel::getSelectedCompositionIndex() const {
        i32 index = 0;
        for (auto& [compositionID, composition] : compositions) {
            const bool isSelected = compositionID == currentCompositionID;
            if (isSelected) {
                return index;
            }
            if (ImGui::Selectable(compositionID.c_str(), isSelected)) {
                app.undoStack.push<ChangeCurrentComposition>(compositionID);
            }
            index++;
        }
        return -1;
    }

    void ViewportEditionPanel::saveCompositions() {
        namespace fs = std::filesystem;

        // make backups
        // clear
        auto compositionsFolder = GetVFS().resolve(Carrot::IO::VFS::Path{"game", "compositions"});
        auto compositionsBackupFolder = GetVFS().resolve(Carrot::IO::VFS::Path{"game", ".backup-compositions"});
        fs::create_directories(compositionsBackupFolder);
        if (fs::exists(compositionsFolder)) {
            fs::copy(compositionsFolder, compositionsBackupFolder, fs::copy_options::recursive | fs::copy_options::overwrite_existing);

            fs::remove_all(compositionsFolder);
        } else {
            fs::create_directories(compositionsFolder);
        }

        for (const auto& [name, composition] : compositions) {
            composition.serialize((compositionsFolder / name).replace_extension(".toml"));
        }
        fs::remove_all(compositionsBackupFolder);
    }

}