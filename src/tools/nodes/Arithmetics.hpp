//
// Created by jglrxavpok on 03/05/2021.
//

#pragma once

#include "../EditorNode.h"

namespace Tools {
    class AddNode: public EditorNode {
    public:
        explicit AddNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Add", "add") {
            init();
        }

        explicit AddNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Add", "add", json) {
            init();
        }

    private:
        void init() {
            newInput("Base");
            newInput("Append");

            newOutput("Result");
        }
    };

    class SubNode: public EditorNode {
    public:
        explicit SubNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Subtract", "sub") {
            init();
        }

        explicit SubNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Subtract", "sub", json) {
            init();
        }

    private:
        void init() {
            newInput("Base");
            newInput("Append");

            newOutput("Result");
        }
    };

    class MultNode: public EditorNode {
    public:
        explicit MultNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Multiply", "mult") {
            init();
        }

        explicit MultNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Multiply", "mult", json) {
            init();
        }

    private:
        void init() {
            newInput("Base");
            newInput("Append");

            newOutput("Result");
        }
    };

    class DivNode: public EditorNode {
    public:
        explicit DivNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Divide", "div") {
            init();
        }

        explicit DivNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Divide", "div", json) {
            init();
        }

    private:
        void init() {
            newInput("Base");
            newInput("Append");

            newOutput("Result");
        }
    };
}
