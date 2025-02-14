//
// Created by jglrxavpok on 03/02/2025.
//
// Converts "legacy" .json scenes to next structure (folders/.toml)

#include "SceneConverter.h"
#include <rapidjson/document.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <core/Macros.h>
#include <core/containers/Vector.hpp>
#include <core/io/FileHandle.h>
#include <core/io/IO.h>
#include <core/utils/stringmanip.h>
#include <core/utils/UUID.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <core/utils/TOML.h>

namespace fs = std::filesystem;

// just enough to convert
struct EntityRepresentation {
    Carrot::UUID parent = Carrot::UUID::null();
    Carrot::Vector<Carrot::UUID> children;
    std::string flags;
    std::string name;
    std::unordered_map<std::string, rapidjson::Value> components;
};

struct SceneRepresentation {
    std::unordered_map<Carrot::UUID, EntityRepresentation> entities;
};

void sanitizeName(std::string& s) {
    for (char& c : s) {
        switch (c) {
            case '"':
            case '/':
            case '\\':
            case ':':
            case '*':
            case '?':
            case '|':
            case '<':
            case '>':
                c = '#';
                break;

            default:
                break;
        }
    }
}

std::unique_ptr<toml::node> json2toml(const rapidjson::Value& v) {
    if (v.IsArray()) {
        auto pArray = std::make_unique<toml::array>();
        for (const auto& elem : v.GetArray()) {
            pArray->push_back(*json2toml(elem));
        }
        return pArray;
    } else if (v.IsObject()) {
        auto pTable = std::make_unique<toml::table>();
        for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it) {
            pTable->insert(it->name.GetString(), *json2toml(it->value));

        }
        return pTable;
    } else if (v.IsBool()) {
        return std::make_unique<toml::value<bool>>(v.GetBool());
    } else if (v.IsDouble()) {
        return std::make_unique<toml::value<double>>(v.GetDouble());
    } else if (v.IsInt()) {
        return std::make_unique<toml::value<std::int64_t>>(v.GetInt());
    } else if (v.IsInt64()) {
        return std::make_unique<toml::value<std::int64_t>>(v.GetInt64());
    } else if (v.IsUint()) {
        return std::make_unique<toml::value<std::int64_t>>(v.GetUint());
    } else if (v.IsUint64()) {
        return std::make_unique<toml::value<std::int64_t>>(v.GetUint64());
    } else if (v.IsNull()) {
        TODO;
    } else if (v.IsString()) {
        return std::make_unique<toml::value<std::string>>(v.GetString());
    }
    TODO;
}

void Carrot::SceneConverter::convert(const fs::path& scenePath, const fs::path& outputRoot) {
    std::cout << scenePath << std::endl;
    std::string sceneName = scenePath.stem().string();
    auto sceneFolder = outputRoot / sceneName;
    fs::create_directories(sceneFolder);

    rapidjson::Document sceneDocument;
    Carrot::IO::FileHandle sceneFile {scenePath, Carrot::IO::OpenMode::Read};
    auto fileContents = sceneFile.readAll();
    std::string fileContentsAsText { (char*)fileContents.get(), sceneFile.getSize() };
    sceneDocument.Parse(fileContentsAsText);

    SceneRepresentation scene;
    if (sceneDocument.HasMember("entities")) {
        // read all entities of this scene
        auto& entityList = sceneDocument["entities"];
        for (auto memberIt = entityList.MemberBegin(); memberIt != entityList.MemberEnd(); ++memberIt) {
            auto& entityObj = memberIt->value;
            Carrot::UUID entityUUID = Carrot::UUID::fromString(memberIt->name.GetString());
            EntityRepresentation& entity = scene.entities[entityUUID];

            entity.name = entityObj["name"].GetString();
            sanitizeName(entity.name);
            if (entityObj.HasMember("parent")) {
                entity.parent = Carrot::UUID::fromString(entityObj["parent"].GetString());
            }
            if (entityObj.HasMember("flags")) {
                auto flagStrings = Carrot::splitString(entityObj["flags"].GetString(), "|");
                std::vector<std::string_view> flagsAsSV { flagStrings.size() };
                for (int i = 0; i < flagStrings.size(); i++) {
                    flagsAsSV[i] = flagStrings[i];
                }
                entity.flags = Carrot::joinStrings(flagsAsSV, "\n");
            }

            for (auto componentIt = entityObj.MemberBegin(); componentIt != entityObj.MemberEnd(); ++componentIt) {
                std::string componentName = componentIt->name.GetString();
                if (componentName != "name" && componentName != "parent" && componentName != "flags") {
                    entity.components[componentName] = std::move(componentIt->value);
                }
            }
        }

        // link children & parents together
        // find root entities
        Carrot::Vector<Carrot::UUID> rootEntities;
        for (auto& [uuid, entity] : scene.entities) {
            if (entity.parent != Carrot::UUID::null()) {
                scene.entities[entity.parent].children.pushBack(uuid);
            } else {
                rootEntities.pushBack(uuid);
            }
        }

        // write entity tree
        std::function<void(const Carrot::Vector<Carrot::UUID>&)> recursiveWriteEntities = [&](const Carrot::Vector<Carrot::UUID>& entityUUIDs) {
            // TODO: check duplicated names

            for (const auto& entityUUID : entityUUIDs) {
                const auto& entity = scene.entities[entityUUID];
                fs::path localPath = entity.name;

                Carrot::UUID parentUUID = entity.parent;
                while (parentUUID != Carrot::UUID::null()) {
                    const auto& parentEntity = scene.entities[parentUUID];
                    localPath = parentEntity.name / localPath;
                    parentUUID = parentEntity.parent;
                }

                fs::path entityFolder = sceneFolder / localPath;
                fs::create_directories(entityFolder);

                fs::path uuidPath = entityFolder / ".uuid";
                Carrot::IO::writeFile(uuidPath.string(), [&](std::ofstream& out) { out << entityUUID.toString(); });

                fs::path flagsPath = entityFolder / ".flags";
                Carrot::IO::writeFile(flagsPath.string(), [&](std::ofstream& out) { out << entity.flags; });

                for (const auto& [componentName, componentData] : entity.components) {
                    auto componentTOML = *dynamic_cast<toml::table*>(json2toml(componentData).get());

                    fs::path path = entityFolder / componentName;
                    path += ".toml";
                    std::ofstream out{path, std::ios::binary};
                    out << componentTOML;
                }

                recursiveWriteEntities(entity.children);
            }
        };
        recursiveWriteEntities(rootEntities);
    }

    auto convertToTOML = [&](const char* member, const fs::path& output) {
        if (sceneDocument.HasMember(member)) {
            std::ofstream out{output, std::ios::binary};
            auto tomlData = *dynamic_cast<toml::table*>(json2toml(sceneDocument[member]).get());
            out << tomlData;
        }
    };
    convertToTOML("world_data", sceneFolder / "WorldData.toml");
    convertToTOML("lighting", sceneFolder / "Lighting.toml");
    if (sceneDocument.HasMember("skybox")) {
        std::string name = sceneDocument["skybox"].GetString();
        std::ofstream out{sceneFolder / "Skybox.toml", std::ios::binary};
        toml::table tomlData;
        tomlData.insert("name", toml::value(name));
        out << tomlData;
    }

    if (sceneDocument.HasMember("logic_systems")) {
        auto& systems = sceneDocument["logic_systems"];
        for (auto it = systems.MemberBegin(); it != systems.MemberEnd(); ++it) {
            auto componentTOML = *dynamic_cast<toml::table*>(json2toml(it->value).get());

            fs::path path = sceneFolder / ".LogicSystems" / it->name.GetString();
            path += ".toml";
            if (!fs::exists(path.parent_path())) {
                fs::create_directories(path.parent_path());
            }
            std::ofstream out{path, std::ios::binary};
            out << componentTOML;
        }
    }
    if (sceneDocument.HasMember("render_systems")) {
        auto& systems = sceneDocument["render_systems"];
        for (auto it = systems.MemberBegin(); it != systems.MemberEnd(); ++it) {
            auto componentTOML = *dynamic_cast<toml::table*>(json2toml(it->value).get());

            fs::path path = sceneFolder / ".RenderSystems" / it->name.GetString();
            path += ".toml";
            if (!fs::exists(path.parent_path())) {
                fs::create_directories(path.parent_path());
            }
            std::ofstream out{path, std::ios::binary};
            out << componentTOML;
        }
    }
}