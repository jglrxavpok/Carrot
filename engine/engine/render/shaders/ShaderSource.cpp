//
// Created by jglrxavpok on 28/11/2021.
//

#include "ShaderSource.h"
#include "core/data/ShaderMetadata.h"
#include "engine/utils/Macros.h"
#include "core/io/IO.h"
#include "core/io/Logging.hpp"
#include "engine/utils/Macros.h"
#include "engine/Engine.h"

namespace Carrot::Render {
    ShaderSource::ShaderSource() {
    }

    ShaderSource::ShaderSource(const std::string& filename): ShaderSource(Carrot::IO::Resource(filename)) {

    }

    ShaderSource::ShaderSource(const char* filename): ShaderSource(std::string(filename)) {

    }

    ShaderSource::ShaderSource(const ShaderSource& toCopy) {
        fromFile = toCopy.fromFile;
        filepath = toCopy.filepath;
        rawData = toCopy.rawData;

        if(fromFile) {
            createWatcher();
        }
    }

    ShaderSource::ShaderSource(const Carrot::IO::Resource& resource) {
        if(resource.isFile()) {
            fromFile = true;
            filepath = resource.getName();

            createWatcher();
        } else {
            fromFile = false;
            rawData.resize(resource.getSize());
            resource.readAll(rawData.data());
        }
    }

    void ShaderSource::createWatcher() {
        verify(fromFile, "Must be from file");
        std::filesystem::path metadataPath = filepath;
        metadataPath.replace_extension(".meta.json");
        std::string contents = Carrot::IO::Resource(metadataPath.string()).readText();

        rapidjson::Document json;
        json.Parse(contents.c_str());

        ShaderCompiler::Metadata metadata(json);

        Carrot::Log::debug("Creating watcher on file '%s'", filepath.u8string().c_str());

        watcher = GetEngine().createFileWatcher([this, commandArgs = metadata.commandArguments](const std::filesystem::path& p) {
            std::string command = "./shadercompiler";
            for(size_t i = 0; i < commandArgs.size(); i++) {
                command += " ";
                command += "\"";
                command += commandArgs[i];
                command += "\"";
            }
            Carrot::Log::debug("Detected shader source change. Launching command %s", command.c_str());
            int ret = system(command.c_str());
            sourceModified = true;
        }, metadata.sourceFiles);
    }

    bool ShaderSource::hasSourceBeenModified() const {
        return sourceModified;
    }

    void ShaderSource::clearModifyFlag() {
        sourceModified = false;
    }

    std::vector<std::uint8_t> ShaderSource::getCode() const {
        if(!fromFile) {
            return rawData;
        } else {
            Carrot::IO::Resource resource(filepath.string());
            std::vector<std::uint8_t> data;
            data.resize(resource.getSize());
            resource.readAll(data.data());
            return data;
        }
    }

    std::string ShaderSource::getName() const {
        if(!fromFile) {
            return "<<from memory>>";
        } else {
            std::u8string u8string = filepath.u8string();
            return Carrot::toString(u8string);
        }
    }

    ShaderSource::operator bool() const {
        return fromFile || !rawData.empty();
    }
}