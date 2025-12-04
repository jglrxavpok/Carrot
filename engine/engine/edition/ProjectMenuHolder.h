//
// Created by jglrxavpok on 28/05/2021.
//

#pragma once
#include "imgui.h"
#include "EditorSettings.h"
#include <nfd.h>
#include <engine/render/RenderContext.h>
#include <core/utils/stringmanip.h>
#include <IconsFontAwesome6.h>
#include <engine/io/actions/ActionSet.h>

namespace Tools {
    class ProjectMenuHolder {
    public:
        static std::filesystem::path EmptyProject;

        explicit ProjectMenuHolder() {}

        void attachSettings(EditorSettings& settings) {
            this->settings = &settings;
            popupName = "Unsaved changes##" + settings.getName();
            newItemName = ICON_FA_FILE "  New##" + settings.getName();
            openItemName = ICON_FA_FOLDER_OPEN "  Open##" + settings.getName();
            openRecentItemName = ICON_FA_FOLDER_OPEN "  Open Recent##" + settings.getName();
            saveItemName = ICON_FA_FLOPPY_DISK "  Save##" + settings.getName();
            saveAsItemName = ICON_FA_FLOPPY_DISK "  Save as...##" + settings.getName();

            initShortcuts();
        }

        /// Loads the shortcuts for this editor
        virtual void initShortcuts();

        /// Checks if shortcuts are being used, and call corresponding code for each shortcut
        virtual void handleShortcuts(const Carrot::Render::Context& frame);

        virtual void onCutShortcut(const Carrot::Render::Context& frame);
        virtual void onCopyShortcut(const Carrot::Render::Context& frame);
        virtual void onPasteShortcut(const Carrot::Render::Context& frame);
        virtual void onDuplicateShortcut(const Carrot::Render::Context& frame);
        virtual void onDeleteShortcut(const Carrot::Render::Context& frame);
        virtual void onUndoShortcut(const Carrot::Render::Context& frame);
        virtual void onRedoShortcut(const Carrot::Render::Context& frame);

        void drawProjectMenu() {
            assert(settings);
            if(ImGui::MenuItem(newItemName.c_str())) {
                scheduleLoad(EmptyProject);
            }
            if(ImGui::MenuItem(openItemName.c_str())) {
                nfdchar_t* outPath;

                // prepare filters for the dialog
                nfdfilteritem_t filterItem[1] = {{"Project", "json"}};

                // show the dialog
                nfdresult_t result = NFD_OpenDialog(&outPath, filterItem, 1, nullptr);
                if (result == NFD_OKAY) {
                    scheduleLoad(outPath);
                    // remember to free the memory (since NFD_OKAY is returned)
                    NFD_FreePath(outPath);
                } else if (result == NFD_CANCEL) {
                    // no-op
                } else {
                    std::string msg = "Error: ";
                    msg += NFD_GetError();
                    throw std::runtime_error(msg);
                }
            }
            if(ImGui::BeginMenu(openRecentItemName.c_str(), !settings->recentProjects.empty())) {
                auto recentProjectsCopy = settings->recentProjects;
                for(const auto& project : recentProjectsCopy) {
                    if(ImGui::MenuItem(project.string().c_str())) {
                        scheduleLoad(project);
                    }
                }

                ImGui::EndMenu();
            }

            if(ImGui::MenuItem(saveItemName.c_str())) {
                if(triggerSave()) {
                    settings->addToRecentProjects(settings->currentProject.value());
                }
            }

            if(ImGui::MenuItem(saveAsItemName.c_str())) {
                std::optional<std::filesystem::path> prevProject = settings->currentProject;
                settings->currentProject = "";
                bool result = triggerSaveAs(settings->currentProject.value());
                if(!result) {
                    settings->currentProject = prevProject;
                } else {
                    auto savedAs = settings->currentProject.value();
                    settings->addToRecentProjects(savedAs);

                    currentProjectName = Carrot::toString(savedAs.stem().u8string());
                }
            }
        }

        void openUnsavedChangesPopup(const std::function<void()>& onContinue, const std::function<void()>& onCancel) {
            if(!showUnsavedChangesPopup()) {
                onContinue();
                return;
            }
            this->onContinueAction = onContinue;
            this->onCancelAction = onCancel;
            unsavedChangePopupIsOpen = true;
        }

        virtual void onFrame(const Carrot::Render::Context& renderContext) {
            assert(settings);
            if(cantSavePopup) {
                tryingToOpenFile = false;
                fileToOpen = EmptyProject;
            }
            if(tryingToOpenFile) {
                openUnsavedChangesPopup([&]() {
                                            performLoad();
                                            tryingToOpenFile = false;
                                        },
                                        [&]() {
                                            fileToOpen = "";
                                            tryingToOpenFile = false;
                                        });
            }

            bool isOpen = true;
            if(unsavedChangePopupIsOpen) {
                ImGui::OpenPopup(popupName.c_str());
                if(ImGui::BeginPopupModal(popupName.c_str(), &isOpen)) {
                    ImGui::Text("You currently have unsaved changes!");
                    ImGui::Text("Do you still want to continue?");

                    if(ImGui::Button(ICON_FA_FLOPPY_DISK "  Save")) {
                        if(triggerSave()) {
                            onContinueAction();
                        } else {
                            onCancelAction();
                        }
                        unsavedChangePopupIsOpen = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if(ImGui::Button(ICON_FA_CROSS "  Don't save")) {
                        onContinueAction();
                        unsavedChangePopupIsOpen = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if(ImGui::Button(ICON_FA_BAN "  Cancel")) {
                        onCancelAction();
                        unsavedChangePopupIsOpen = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
                if(!isOpen) {
                    unsavedChangePopupIsOpen = false;
                }
            }

            if (cantSavePopup) {
                cantSavePopup = drawCantSavePopup();
            }
        }

        bool triggerSaveAs(std::filesystem::path& path) {
            if(!canSave()) {
                handleCantSavePopup();
                return false;
            }
            nfdchar_t* savePath;

            // prepare filters for the dialog
            nfdfilteritem_t filterItem[1] = {{"Project", "json"}};

            // show the dialog
            nfdresult_t result = NFD_SaveDialog(&savePath, filterItem, 1, nullptr, "Untitled.json");
            if (result == NFD_OKAY) {
                saveToFile(savePath);
                path = savePath;
                // remember to free the memory (since NFD_OKAY is returned)
                NFD_FreePath(savePath);
                return true;
            } else if (result == NFD_CANCEL) {
                return false;
            } else {
                std::string msg = "Error: ";
                msg += NFD_GetError();
                throw std::runtime_error(msg);
            }
        }

        bool triggerSave() {
            assert(settings);
            if(!canSave()) {
                handleCantSavePopup();
                return false;
            }
            if(settings->currentProject) {
                saveToFile(settings->currentProject.value());
                return true;
            } else {
                std::optional<std::filesystem::path> prevProject = settings->currentProject;
                settings->currentProject = "";
                bool result = triggerSaveAs(settings->currentProject.value());
                if(!result) {
                    settings->currentProject = prevProject;
                }
                return result;
            }
        }

        virtual void performLoad(std::filesystem::path path) = 0;

        virtual void saveToFile(std::filesystem::path path) = 0;
        virtual bool showUnsavedChangesPopup() = 0;
        virtual bool canSave() const { return true; };
        bool hasUnsavedPopupOpen() const { return unsavedChangePopupIsOpen; }

        virtual bool drawCantSavePopup() {
            return true;
        }

        virtual ~ProjectMenuHolder() = default;

        const std::string& getCurrentProjectName() const {
            return currentProjectName;
        }

        const std::filesystem::path& getCurrentProjectFile() const {
            return currentFile;
        }

        void scheduleLoad(std::filesystem::path path) {
            if(!canSave()) {
                handleCantSavePopup();
                return;
            }
            fileToOpen = path;
            if(showUnsavedChangesPopup()) {
                ImGui::OpenPopup(popupName.c_str());
                tryingToOpenFile = true;
            } else {
                performLoad();
            }
        }

    private:
        void handleCantSavePopup() {
            cantSavePopup = true;
        }

        void performLoad() {
            performLoad(fileToOpen);
            currentFile = fileToOpen;
            currentProjectName = currentFile.stem().string();
            fileToOpen = EmptyProject;
            tryingToOpenFile = false;
        }

    private:
        EditorSettings* settings = nullptr;
        std::string popupName;
        std::string newItemName;
        std::string openItemName;
        std::string openRecentItemName;
        std::string saveItemName;
        std::string saveAsItemName;

        std::filesystem::path fileToOpen = EmptyProject;
        std::filesystem::path currentFile = EmptyProject;
        std::string currentProjectName = "Untitled";

        bool unsavedChangePopupIsOpen = false;
        std::function<void()> onContinueAction;
        std::function<void()> onCancelAction;

        Carrot::IO::ActionSet shortcuts{"editor_shortcuts"};
        Carrot::IO::BoolInputAction duplicate{"duplicate"};
        Carrot::IO::BoolInputAction copy{"copy"};
        Carrot::IO::BoolInputAction cut{"cut"};
        Carrot::IO::BoolInputAction paste{"paste"};
        Carrot::IO::BoolInputAction deleteShortcut{"delete"};
        Carrot::IO::BoolInputAction undoShortcut{"undo"};
        Carrot::IO::BoolInputAction redoShortcut{"redo"};

    private: // popup control
        bool tryingToOpenFile = false;
        bool cantSavePopup = false;
    };
}
