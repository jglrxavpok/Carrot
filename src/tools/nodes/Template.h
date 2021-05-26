//
// Created by jglrxavpok on 26/05/2021.
//

#pragma once
#include "../EditorNode.h"
#include "engine/io/IO.h"
#include <filesystem>

namespace Tools {
    class TemplateParseError: public std::exception {
    public:
        explicit TemplateParseError(const std::string& message, const std::string& templateName, const char* file, const char* function, int line) {
            fullMessage = "Error in template '" + templateName + "': " + file + ", in " + function + " (line " + std::to_string(line) + ")";
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
        explicit TemplateNode(Tools::EditorGraph& graph, const std::string& name): EditorNode(graph, name, "template"), templateName(name) {
            load();
        }

        explicit TemplateNode(Tools::EditorGraph& graph, const rapidjson::Value& json): EditorNode(graph, "tmp-template", "template", json) {
            templateName = json["template_name"].GetString();
            load();
        }

    public:
        rapidjson::Value serialiseToJSON(rapidjson::Document& doc) const override {
            // TODO
            return EditorNode::serialiseToJSON(doc);
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex) const override {
            throw "TODO";
        }

    private:
        inline static std::array<std::string, 2> Paths = {
                "resources/node_templates/", // default builtin templates
                "user/node_templates/", // user defined templates (TODO: move elsewhere?)
        };

#define THROW_IF(condition, message) if(condition) throw TemplateParseError(message, templateName, __FILE__, __FUNCTION__, __LINE__);

        void load() {
            rapidjson::Document description;
            for(const auto& pathPrefix : Paths) {
                if(std::filesystem::exists(pathPrefix)) {
                    description.Parse(IO::readFileAsText(pathPrefix + templateName + ".json").c_str());
                    break;
                }
            }

            THROW_IF(!description.HasMember("title"), "Missing 'title' member");
            THROW_IF(!description.HasMember("inputs"), "Missing 'inputs' member");
            THROW_IF(!description.HasMember("outputs"), "Missing 'outputs' member");
            title = description["title"].GetString();

            auto inputArray = description["inputs"].GetArray();
            for(const auto& input : inputArray) {
                THROW_IF(!input.HasMember("name"), "Missing 'name' member inside input");
                THROW_IF(!input.HasMember("type"), "Missing 'type' member inside input");
                auto type = Carrot::ExpressionTypes::fromName(input["type"].GetString());
                newInput(input["name"].GetString(), type);
            }

            auto outputArray = description["outputs"].GetArray();
            for(const auto& output : outputArray) {
                THROW_IF(!output.HasMember("name"), "Missing 'name' member inside output");
                THROW_IF(!output.HasMember("type"), "Missing 'type' member inside output");
                auto type = Carrot::ExpressionTypes::fromName(output["type"].GetString());
                newOutput(output["name"].GetString(), type);
            }
        }

    private:
        std::string templateName;
    };
}
