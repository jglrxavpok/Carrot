//
// Created by jglrxavpok on 24/06/25.
//

#include <Fertilizer.h>
#include <fstream>
#include <ParticleProcessing.h>
#include <core/data/ParticleBlueprint.h>
#include <node_based/EditorGraph.h>
#include <node_based/generators/ParticleShaderGenerator.h>
#include <node_based/nodes/DefaultNodes.h>

namespace Fertilizer {
    ConversionResult processParticleFile(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile) {
        EditorGraph updateGraph{"update"};
        EditorGraph renderGraph{"render"};

        Nodes::addParticleEditorNodesForRenderGraph(renderGraph);
        Nodes::addParticleEditorNodesForUpdateGraph(updateGraph);

        const std::string filepathAsString = inputFile.string();
        rapidjson::Document description;
        description.Parse(Carrot::IO::readFileAsText(filepathAsString).c_str());

        updateGraph.loadFromJSON(description["update_graph"]);
        renderGraph.loadFromJSON(description["render_graph"]);

        std::unordered_set<Carrot::UUID> activeLinks; // not read from during generation (only used for visualisation)
        auto updateExpressions = updateGraph.generateExpressionsFromTerminalNodes(activeLinks);
        auto fragmentExpressions = renderGraph.generateExpressionsFromTerminalNodes(activeLinks);
        ParticleShaderGenerator updateGenerator(ParticleShaderMode::Compute, filepathAsString);
        ParticleShaderGenerator fragmentGenerator(ParticleShaderMode::Fragment, filepathAsString);

        auto computeShader = updateGenerator.compileToSPIRV(updateExpressions);
        auto fragmentShader = fragmentGenerator.compileToSPIRV(fragmentExpressions);
        bool isOpaque = false; // TODO: determine via render graph

        Carrot::ParticleBlueprint blueprint(std::move(computeShader), std::move(fragmentShader), isOpaque);

        Carrot::IO::writeFile(outputFile.string(), [&](std::ostream& out) {
            out << blueprint;
        });

        return ConversionResult { ConversionResultError::Success };
    }
}
