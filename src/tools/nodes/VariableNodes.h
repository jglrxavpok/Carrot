//
// Created by jglrxavpok on 19/05/2021.
//

#pragma once

#include "tools/EditorNode.h"

namespace Tools {
    enum class VariableNodeType {
        GetVelocity,
        GetSize,
        GetLife,
        GetParticleID,
        GetDeltaTime,
        GetPosition,
        // TODO
    };

    class VariableNode : public EditorNode {
    private:
        VariableNodeType nodeType;
        uint32_t dimensions = 0;

    public:
        explicit VariableNode(Tools::EditorGraph& graph, VariableNodeType type): Tools::EditorNode(graph, getTitle(type), getInternalName(type)), nodeType(type) {
                init();
        }

        explicit VariableNode(Tools::EditorGraph& graph, VariableNodeType type, const rapidjson::Value& json): Tools::EditorNode(graph, getTitle(type), getInternalName(type), json), nodeType(type) {
                init();
        }

    public:
        inline VariableNodeType getTerminalType() const { return nodeType; };

        inline std::shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex) const {
            return make_shared<Carrot::GetVariableExpression>(getInternalName(nodeType), outputIndex);
        };

        inline static std::string getTitle(VariableNodeType type) {
            switch (type) {
                case VariableNodeType::GetVelocity:
                    return "Get Velocity";
                case VariableNodeType::GetSize:
                    return "Get Size";
                case VariableNodeType::GetLife:
                    return "Get Life";
                case VariableNodeType::GetParticleID:
                    return "Get ParticleID";
                case VariableNodeType::GetDeltaTime:
                    return "Get DeltaTime";
                case VariableNodeType::GetPosition:
                    return "Get Position";
            }
            throw std::runtime_error("Unsupported terminal node type");
        }

        inline static std::string getInternalName(VariableNodeType type) {
            switch (type) {
                case VariableNodeType::GetVelocity:
                    return "get_velocity";
                case VariableNodeType::GetSize:
                    return "get_size";
                case VariableNodeType::GetLife:
                    return "get_life";
                case VariableNodeType::GetParticleID:
                    return "get_particle_id";
                case VariableNodeType::GetDeltaTime:
                    return "get_delta_time";
                case VariableNodeType::GetPosition:
                    return "get_position";
            }
            throw std::runtime_error("Unsupported terminal node type");
        }

        inline static uint32_t getDimensionCount(VariableNodeType type) {
            switch (type) {
                case VariableNodeType::GetVelocity:
                case VariableNodeType::GetPosition:
                    return 3;
                default:
                    return 1;
            }
        }

    private:
        inline static const char* DIMENSION_NAMES[] = {
                "X", "Y", "Z", "W"
        };

        void init() {
            dimensions = getDimensionCount(nodeType);
            assert(dimensions <= 4);
            for(int i = 0; i < dimensions; i++) {
                newOutput(DIMENSION_NAMES[i]);
            }
        }
    };
}