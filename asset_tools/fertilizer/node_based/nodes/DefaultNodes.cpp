//
// Created by jglrxavpok on 24/06/25.
//

#include "DefaultNodes.h"
#include <node_based/EditorGraph.h>

#include "Arithmetics.hpp"
#include "BuiltinFunctions.h"
#include "CommentNode.h"
#include "Constants.hpp"
#include "Logics.hpp"

namespace Fertilizer::Nodes {
    void addCommonOperators(EditorGraph& graph) {
        graph.addToLibrary<AddNode>("add", "Add");
        graph.addToLibrary<SubNode>("sub", "Subtract");
        graph.addToLibrary<MultNode>("mult", "Multiply");
        graph.addToLibrary<DivNode>("div", "Divide");
        graph.addToLibrary<ModNode>("mod", "Modulus");
        graph.addToLibrary<MinNode>("min", "Minimum");
        graph.addToLibrary<MaxNode>("max", "Maximum");
    }

    void addCommonMath(EditorGraph& graph) {
        graph.addToLibrary<CosNode>("cos", "Cos");
        graph.addToLibrary<SinNode>("sin", "Sin");
        graph.addToLibrary<TanNode>("tan", "Tan");
        graph.addToLibrary<ExpNode>("exp", "Exp");
        graph.addToLibrary<AbsNode>("abs", "Abs");
        graph.addToLibrary<LogNode>("log", "Log");
        graph.addToLibrary<SqrtNode>("sqrt", "Sqrt");
    }

    void addCommonLogic(EditorGraph& graph) {
        graph.addToLibrary<LessNode>("float_less", "Less");
        graph.addToLibrary<LessOrEqualsNode>("float_less_or_equals", "Less or Equals");
        graph.addToLibrary<GreaterNode>("float_greater", "Greater");
        graph.addToLibrary<GreaterOrEqualsNode>("float_greater_or_equals", "Greater or Equals");
        graph.addToLibrary<EqualsNode>("float_equals", "Equals");
        graph.addToLibrary<NotEqualsNode>("float_not_equals", "Not Equals");

        graph.addToLibrary<AndNode>("and", "And");
        graph.addToLibrary<OrNode>("or", "Or");
        graph.addToLibrary<XorNode>("xor", "Xor");
        graph.addToLibrary<NegateBoolNode>("not", "Not");
    }

    void addCommonInputs(EditorGraph& graph) {
        graph.addToLibrary<FloatConstantNode>("float_constant", "Float Constant");
        graph.addToLibrary<BoolConstantNode>("bool_constant", "Bool Constant");
        graph.addVariableToLibrary<VariableNodeType::GetSize>();
        graph.addVariableToLibrary<VariableNodeType::GetLife>();
        graph.addVariableToLibrary<VariableNodeType::GetParticleIndex>();
        graph.addVariableToLibrary<VariableNodeType::GetEmissionID>();
        graph.addVariableToLibrary<VariableNodeType::GetVelocity>();
        graph.addVariableToLibrary<VariableNodeType::GetPosition>();
    }

    void addCommonParticleEditorNodes(EditorGraph& graph) {
        {
            NodeLibraryMenuScope s1("Operators", &graph);
            Nodes::addCommonOperators(graph);
        }
        {
            NodeLibraryMenuScope s1("Logic", &graph);
            Nodes::addCommonLogic(graph);
        }

        {
            NodeLibraryMenuScope s1("Functions", &graph);
            Nodes::addCommonMath(graph);
        }

        {
            NodeLibraryMenuScope s1("Inputs", &graph);
            Nodes::addCommonInputs(graph);
        }

        graph.addToLibrary<CommentNode>("comment", "Comment");

        graph.addTemplatesToLibrary();
        graph.addTemplateSupport();
    }

    void addParticleEditorNodesForUpdateGraph(EditorGraph& graph) {
        addCommonParticleEditorNodes(graph);

        {
            NodeLibraryMenuScope s1("Update Inputs", &graph);
            graph.addVariableToLibrary<VariableNodeType::GetDeltaTime>();
        }
        {
            NodeLibraryMenuScope s1("Update Outputs", &graph);
            graph.addToLibrary<TerminalNodeType::SetVelocity>();
            graph.addToLibrary<TerminalNodeType::SetSize>();
        }
    }

    void addParticleEditorNodesForRenderGraph(EditorGraph& graph) {
        addCommonParticleEditorNodes(graph);

        {
            NodeLibraryMenuScope s2("Render Inputs", &graph);
            graph.addVariableToLibrary<VariableNodeType::GetFragmentPosition>();
        }
        {
            NodeLibraryMenuScope s2("Render Outputs", &graph);
            graph.addToLibrary<TerminalNodeType::SetOutputColor>();
            graph.addToLibrary<TerminalNodeType::DiscardPixel>();
        }
    }
}
