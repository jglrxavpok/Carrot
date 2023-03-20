//
// Created by jglrxavpok on 20/03/2023.
//

#include "CSProject.h"
#include "core/io/Logging.hpp"
#include <core/utils/stringmanip.h>
#include <rapidxml.hpp>

namespace Carrot::Scripting {

    CSProject::CSProject(Carrot::IO::Resource csprojFile) {
        using namespace rapidxml;

        xml_document<> doc;
        std::string text = csprojFile.readText();
        doc.parse<0>(text.data());

        xml_node<>* project = doc.first_node("Project");
        if(!project) {
            Carrot::Log::error("No <Project> inside %s", csprojFile.getName().c_str());
            return;
        }

        for(auto* itemGroup = project->first_node("ItemGroup"); itemGroup != nullptr; itemGroup = itemGroup->next_sibling("ItemGroup")) {
            for(auto* compileItem = itemGroup->first_node("Compile"); compileItem != nullptr; compileItem = compileItem->next_sibling("Compile")) {
                auto* includeAttr = compileItem->first_attribute("Include");
                if(includeAttr) {
                    sourceFiles.emplace_back(includeAttr->value());
                }
            }
        }
    }

    std::span<const std::filesystem::path> CSProject::getSourceFiles() const {
        return sourceFiles;
    }
} // Carrot::Scripting