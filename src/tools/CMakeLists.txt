set(ToolsRoot "")

set(ToolsSources
        ${ToolsRoot}ParticleEditor.cpp
        ${ToolsRoot}EditorSettings.cpp
        ${ToolsRoot}EditorGraph.cpp
        ${ToolsRoot}EditorNode.cpp
        ${ToolsRoot}TemplateEditor.cpp
        ${ToolsRoot}nodes/TerminalNodes.cpp
        ${ToolsRoot}nodes/NodeEditorUtils.cpp
        ${ToolsRoot}nodes/Template.cpp

        ${ToolsRoot}generators/ParticleShaderGenerator.cpp



        ${ToolsRoot}ToolsEntryPoint.cpp
        )

add_executable(Carrot-Tools ${ToolsSources})
target_link_libraries(Carrot-Tools PUBLIC Engine-Base)

add_executable(Carrot-Tools-Renderdoc ${ToolsSources})
target_link_libraries(Carrot-Tools-Renderdoc PUBLIC Engine-Base-Renderdoc)