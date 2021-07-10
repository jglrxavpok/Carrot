//
// Created by jglrxavpok on 28/05/2021.
//

#pragma once
#include "imgui.h"
#include "EditorSettings.h"
#include <nfd.h>

namespace Tools {
    class ProjectMenuHolder {
    public:
        static std::filesystem::path EmptyProject;

        explicit ProjectMenuHolder() {}

        void attachSettings(EditorSettings& settings) {
            this->settings = &settings;
            popupName = "Unsaved changes##" + settings.getName();
            newItemName = "New##" + settings.getName();
            openItemName = "Open##" + settings.getName();
            openRecentItemName = "Open Recent##" + settings.getName();
            saveItemName = "Save##" + settings.getName();
            saveAsItemName = "Save as...##" + settings.getName();
        }

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
                }
            }
        }

        virtual void onFrame(size_t frameIndex) {
            assert(settings);
            if(tryingToOpenFile) {
                ImGui::OpenPopup(popupName.c_str());
            }
            if(ImGui::BeginPopupModal(popupName.c_str())) {
                ImGui::Text("You currently have unsaved changes!");
                ImGui::Text("Do you still want to continue?");

                if(ImGui::Button("Save")) {
                    if(triggerSave()) {
                        performLoad();
                    }
                    tryingToOpenFile = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if(ImGui::Button("Don't save")) {
                    performLoad();
                    tryingToOpenFile = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if(ImGui::Button("Cancel")) {
                    fileToOpen = "";
                    tryingToOpenFile = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        bool triggerSaveAs(std::filesystem::path& path) {
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

        virtual ~ProjectMenuHolder() = default;

    private:
        void performLoad() {
            performLoad(fileToOpen);
            fileToOpen = EmptyProject;
            tryingToOpenFile = false;
        }

        void scheduleLoad(std::filesystem::path path) {
            fileToOpen = path;
            if(showUnsavedChangesPopup()) {
                ImGui::OpenPopup(popupName.c_str());
                tryingToOpenFile = true;
            } else {
                performLoad();
            }
        }

    private:
        EditorSettings* settings = nullptr;
        std::string popupName;
        std::string newItemName;
        std::string openItemName;
        std::string openRecentItemName;
        std::string saveItemName;
        std::string saveAsItemName;
        bool tryingToOpenFile = false;
        std::filesystem::path fileToOpen = EmptyProject;
    };
}