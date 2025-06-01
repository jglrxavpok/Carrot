set(ToolsRoot "")

set(ToolsSources
        ${ToolsRoot}ParticleEditor.cpp
        ${ToolsRoot}EditorGraph.cpp
        ${ToolsRoot}EditorNode.cpp
        ${ToolsRoot}TemplateEditor.cpp
        ${ToolsRoot}nodes/BinaryFunctionNode.cpp
        ${ToolsRoot}nodes/CommentNode.cpp
        ${ToolsRoot}nodes/TerminalNodes.cpp
        ${ToolsRoot}nodes/NodeEditorUtils.cpp
        ${ToolsRoot}nodes/NodeWithVariants.cpp
        ${ToolsRoot}nodes/Template.cpp

        ${ToolsRoot}generators/ParticleShaderGenerator.cpp



        ${ToolsRoot}ToolsEntryPoint.cpp
        )

add_executable(Carrot-Tools ${ToolsSources})
target_link_libraries(Carrot-Tools PUBLIC Engine-Base)
add_engine_precompiled_headers(Carrot-Tools)