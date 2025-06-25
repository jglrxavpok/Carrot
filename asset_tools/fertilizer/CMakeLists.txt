add_library(fertilizer-lib
        Fertilizer.cpp

        gpu_assistance/VulkanHelper.cpp
        models/ModelProcessing.cpp
        models/GLTFWriter.cpp
        models/MikkTSpaceInterface.cpp

        node_based/generators/ParticleShaderGenerator.cpp
        node_based/EditorGraph.cpp
        node_based/EditorNode.cpp
        node_based/nodes/BinaryFunctionNode.cpp
        node_based/nodes/CommentNode.cpp
        node_based/nodes/DefaultNodes.cpp
        node_based/nodes/TerminalNodes.cpp
        node_based/nodes/NodeEditorUtils.cpp
        node_based/nodes/NodeWithVariants.cpp
        node_based/nodes/Template.cpp

        EnvironmentMapProcessing.cpp
        ParticleProcessing.cpp
        TextureCompression.cpp



        ${ProjectRoot}thirdparty/bc7enc_rdo/rgbcx.cpp
)

target_include_directories(fertilizer-lib PUBLIC ./)
target_include_directories(fertilizer-lib PUBLIC ${ProjectRoot}thirdparty/bc7enc_rdo)

add_core_includes(fertilizer-lib)
target_link_libraries(fertilizer-lib PUBLIC CarrotCore meshoptimizer glslang SPIRV)
add_core_precompiled_headers(fertilizer-lib)

add_executable(fertilizer
        main.cpp
        )
add_core_includes(fertilizer)
target_link_libraries(fertilizer PUBLIC fertilizer-lib)