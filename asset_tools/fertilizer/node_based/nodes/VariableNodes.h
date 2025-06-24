//
// Created by jglrxavpok on 19/05/2021.
//

#pragma once

#include "../EditorNode.h"

namespace Fertilizer {
    enum class VariableNodeType {
        GetVelocity,
        GetSize,
        GetLife,
        GetParticleIndex,
        GetEmissionID,
        GetPosition,
        // COMPUTE
        GetDeltaTime,

        // FRAGMENT
        GetFragmentPosition,
        // TODO
    };

    class VariableNode : public EditorNode {
    private:
        VariableNodeType nodeType;
        std::uint32_t dimensions = 0;

    public:
        explicit VariableNode(Fertilizer::EditorGraph& graph, VariableNodeType type): Fertilizer::EditorNode(graph, getTitle(type), getInternalName(type)), nodeType(type) {
                init();
        }

        explicit VariableNode(Fertilizer::EditorGraph& graph, VariableNodeType type, const rapidjson::Value& json): Fertilizer::EditorNode(graph, getTitle(type), getInternalName(type), json), nodeType(type) {
                init();
        }

    public:
        VariableNodeType getTerminalType() const { return nodeType; };

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex, std::unordered_set<Carrot::UUID>& activeLinks) const {
            return make_shared<Carrot::GetVariableExpression>(getType(nodeType, outputIndex), getInternalName(nodeType), outputIndex);
        };

        static std::string getTitle(VariableNodeType type) {
            switch (type) {
                case VariableNodeType::GetVelocity:
                    return "Get Velocity";
                case VariableNodeType::GetSize:
                    return "Get Size";
                case VariableNodeType::GetLife:
                    return "Get Life";
                case VariableNodeType::GetParticleIndex:
                    return "Get ParticleIndex";
                case VariableNodeType::GetEmissionID:
                    return "Get EmissionID";
                case VariableNodeType::GetDeltaTime:
                    return "Get DeltaTime";
                case VariableNodeType::GetPosition:
                    return "Get Position";
                case VariableNodeType::GetFragmentPosition:
                    return "Get Fragment Position";
            }
            throw std::runtime_error("Unsupported terminal node type");
        }

        static std::string getInternalName(VariableNodeType type) {
            switch (type) {
                case VariableNodeType::GetVelocity:
                    return "get_velocity";
                case VariableNodeType::GetSize:
                    return "get_size";
                case VariableNodeType::GetLife:
                    return "get_life";
                case VariableNodeType::GetEmissionID:
                    return "get_emission_id";
                case VariableNodeType::GetParticleIndex:
                    return "get_particle_index";
                case VariableNodeType::GetDeltaTime:
                    return "get_delta_time";
                case VariableNodeType::GetPosition:
                    return "get_position";
                case VariableNodeType::GetFragmentPosition:
                    return "get_fragment_position";
            }
            throw std::runtime_error("Unsupported terminal node type");
        }

        static Carrot::ExpressionType getType(VariableNodeType type, uint32_t outputIndex) {
            switch (type) {
                case VariableNodeType::GetVelocity:
                case VariableNodeType::GetSize:
                case VariableNodeType::GetLife:
                case VariableNodeType::GetDeltaTime:
                case VariableNodeType::GetPosition:
                case VariableNodeType::GetParticleIndex:
                case VariableNodeType::GetEmissionID:
                case VariableNodeType::GetFragmentPosition:
                    return Carrot::ExpressionTypes::Float;

            }
            throw std::runtime_error("Unsupported terminal node type");
        }

        static std::uint32_t getDimensionCount(VariableNodeType type) {
            switch (type) {
                case VariableNodeType::GetVelocity:
                case VariableNodeType::GetPosition:
                    return 3;
                case VariableNodeType::GetFragmentPosition:
                    return 2;
                default:
                    return 1;
            }
        }

    private:
        inline static const char* DIMENSION_NAMES[] = {
                "X", "Y", "Z", "W"
        };

        inline static const char* TEX_DIMENSION_NAMES[] = {
                "U", "V", "S", "T"
        };

        void init() {
            const char** names = DIMENSION_NAMES;
            switch(nodeType) {
                case VariableNodeType::GetFragmentPosition:
                    names = TEX_DIMENSION_NAMES;
                    break;

                default:
                    break;
            }
            dimensions = getDimensionCount(nodeType);
            assert(dimensions <= 4);
            for(int i = 0; i < dimensions; i++) {
                newOutput(names[i], getType(nodeType, i));
            }
        }
    };
}