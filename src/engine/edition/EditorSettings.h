//
// Created by jglrxavpok on 27/05/2021.
//

#pragma once
#include <string>
#include <fstream>
#include <filesystem>
#include <optional>
#include <list>

namespace Tools {
    class EditorSettings {
    public:
        static constexpr uint64_t MaxRecentFiles = 10;

        explicit EditorSettings(const std::string& name): name(name), settingsFile(name + ".json"), recentProjects(0) {}

        const std::string& getName() const { return name; };

    public:
        void load();
        void save();

        /// Add to recent projects list.
        /// If already present, pushes the project back to the front
        void addToRecentProjects(std::filesystem::path toAdd);

        std::list<std::filesystem::path> recentProjects;
        std::optional<std::filesystem::path> currentProject;
    private:
        std::string name;
        std::filesystem::path settingsFile;
    };
}
