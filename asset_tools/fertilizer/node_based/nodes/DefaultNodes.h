//
// Created by jglrxavpok on 24/06/25.
//

#pragma once
#include <node_based/EditorGraph.h>

namespace Fertilizer {
    class EditorGraph;
}

namespace Fertilizer::Nodes {
    void addCommonOperators(EditorGraph& graph);
    void addCommonMath(EditorGraph& graph);
    void addCommonLogic(EditorGraph& graph);
    void addCommonInputs(EditorGraph& graph);

    void addCommonParticleEditorNodes(EditorGraph& graph);
    void addParticleEditorNodesForUpdateGraph(EditorGraph& graph);
    void addParticleEditorNodesForRenderGraph(EditorGraph& graph);
}