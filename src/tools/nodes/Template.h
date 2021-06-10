//
// Created by jglrxavpok on 26/05/2021.
//

#pragma once
#include "../EditorNode.h"
#include "engine/io/IO.h"
#include "engine/utils/JSON.h"
#include "engine/Engine.h"
#include <filesystem>

namespace Tools {
    class TemplateParseError: public std::exception {
    public:
        explicit TemplateParseError(const std::string& message, const std::string& templateName, const char* file, const char* function, int line, std::string condition) {
            fullMessage = "Error in template '" + templateName + "': " + file + ", in " + function + " (line " + std::to_string(line) + ")\n"
                          "Condition is: " + condition;
        }

        const char *what() const override {
            return fullMessage.c_str();
        }

    private:
        std::string fullMessage;
    };

    /// User-defined black box
    class TemplateNode: public EditorNode {
    public:
        inline static std::array<std::string, 2> Paths = {
                "resources/node_templates", // default builtin templates
                "user/node_templates", // user defined templates (TODO: move elsewhere?)
        };

#define THROW_IF(condition, message) if(condition) throw TemplateParseError(message, templateName, __FILE__, __FUNCTION__, __LINE__, #condition);
    public:
        explicit TemplateNode(Tools::EditorGraph& graph, const std::string& name): EditorNode(graph, name, "template"), templateName(name) {
            load();
        }

        explicit TemplateNode(Tools::EditorGraph& graph, const rapidjson::Value& json): EditorNode(graph, "tmp-template", "template", json) {
            THROW_IF(!json.HasMember("extra"), "Missing 'extra' member inside json object!");
            templateName = json["extra"]["template_name"].GetString();
            load();
        }

    public:
        rapidjson::Value serialiseToJSON(rapidjson::Document& doc) const override {
            return std::move(rapidjson::Value(rapidjson::kObjectType)
                                     .AddMember("template_name", Carrot::JSON::makeRef(templateName), doc.GetAllocator())
            );
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex) const override;

    private:
        void loadInternalGraph(const rapidjson::Value& graphJson);

        void load() {
            rapidjson::Document description;
            for(const auto& pathPrefix : Paths) {
                if(std::filesystem::exists(pathPrefix)) {
                    description.Parse(IO::readFileAsText(pathPrefix + "/" + templateName + ".json").c_str());
                    break;
                }
            }

            inputPinIndices.clear();
            outputPinIndices.clear();

            THROW_IF(!description.HasMember("title"), "Missing 'title' member");
            THROW_IF(!description.HasMember("inputs"), "Missing 'inputs' member");
            THROW_IF(!description.HasMember("outputs"), "Missing 'outputs' member");
            title = description["title"].GetString();

            auto inputArray = description["inputs"].GetArray();
            for(const auto& input : inputArray) {
                THROW_IF(!input.HasMember("name"), "Missing 'name' member inside input");
                THROW_IF(!input.HasMember("type"), "Missing 'type' member inside input");
                THROW_IF(!input.HasMember("dimensions"), "Missing 'dimensions' member inside input");
                auto type = Carrot::ExpressionTypes::fromName(input["type"].GetString());
                uint32_t dimensions = input["dimensions"].GetInt64();
                std::string name = input["name"].GetString();
                if(dimensions == 1) {
                    newInput(name, type);
                    inputPinIndices[name].push_back(inputs.size());
                } else {
                    for (int i = 0; i < dimensions; ++i) {
                        inputPinIndices[name].push_back(inputs.size());
                        newInput(name + "[" + std::to_string(i) + "]", type);
                    }
                }
            }

            auto outputArray = description["outputs"].GetArray();
            for(const auto& output : outputArray) {
                THROW_IF(!output.HasMember("name"), "Missing 'name' member inside output");
                THROW_IF(!output.HasMember("type"), "Missing 'type' member inside output");
                THROW_IF(!output.HasMember("dimensions"), "Missing 'dimensions' member inside output");
                auto type = Carrot::ExpressionTypes::fromName(output["type"].GetString());
                uint32_t dimensions = output["dimensions"].GetInt64();
                std::string name = output["name"].GetString();
                if(dimensions == 1) {
                    outputPinIndices[name].push_back(outputs.size());
                    newOutput(name, type);
                } else {
                    for (int i = 0; i < dimensions; ++i) {
                        outputPinIndices[name].push_back(outputs.size());
                        newOutput(name + "[" + std::to_string(i) + "]", type);
                    }
                }
            }

            loadInternalGraph(description["graph"]);
        }

    private:
        std::string templateName;
        std::unordered_map<std::string, std::vector<std::size_t>> inputPinIndices;
        std::unordered_map<std::string, std::vector<std::size_t>> outputPinIndices;
        std::unique_ptr<EditorGraph> internalGraph = nullptr;
    };
}
