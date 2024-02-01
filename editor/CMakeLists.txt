set(EditorRoot "src/")

set(EditorSources
        ${EditorRoot}Peeler.cpp
        ${EditorRoot}PeelerRendering.cpp
        ${EditorRoot}GridRenderer.cpp
        ${EditorRoot}InspectorWidgets.cpp

        ${EditorRoot}layers/GizmosLayer.cpp
        ${EditorRoot}layers/ISceneViewLayer.cpp

        ${EditorRoot}panels/inspector/AnimatedModelEdition.cpp
        ${EditorRoot}panels/inspector/CSharpEdition.cpp
        ${EditorRoot}panels/inspector/EditorFunctions.cpp
        ${EditorRoot}panels/inspector/ModelEdition.cpp
        ${EditorRoot}panels/inspector/RigidBodyEdition.cpp

        ${EditorRoot}panels/InspectorPanel.cpp
        ${EditorRoot}panels/NavMeshPanel.cpp
        ${EditorRoot}panels/ResourcePanel.cpp

        ${EditorRoot}ecs/systems/CameraRenderer.cpp
        ${EditorRoot}ecs/systems/CharacterPositionSetterSystem.cpp
        ${EditorRoot}ecs/systems/CollisionShapeRenderer.cpp
        ${EditorRoot}ecs/systems/LightEditorRenderer.cpp


        ${EditorRoot}game_specific/ecs/CharacterControllerComponent.cpp
        ${EditorRoot}game_specific/ecs/CharacterControllerSystem.cpp
        )

set(ImGuizmoSources
        ../thirdparty/ImGuizmo/GraphEditor.cpp
        ../thirdparty/ImGuizmo/ImCurveEdit.cpp
        ../thirdparty/ImGuizmo/ImGradient.cpp
        ../thirdparty/ImGuizmo/ImGuizmo.cpp
        ../thirdparty/ImGuizmo/ImSequencer.cpp
        )

set(EditorFragmentShaders
        grid.fragment.glsl
        outlines.fragment.glsl
        )

set(EditorVertexShaders
        grid.vertex.glsl
        )

set(EditorShaders "")

function(add_editor_defines Target)
    target_include_directories("${Target}" PUBLIC ../thirdparty/ImGuizmo)
    target_include_directories("${Target}" PUBLIC src/)
    target_compile_definitions("${Target}" PRIVATE "EDITOR=1") # Used by code to know whether we are inside the editor or not.
endfunction()

foreach(shader ${EditorFragmentShaders})
    add_spirv_shader(fragment "${shader}" "${shader}.spv")
    set(EditorShaders "${EditorShaders}" "${CMAKE_BINARY_DIR}/resources/shaders/${shader}.spv")
endforeach()

foreach(shader ${EditorVertexShaders})
    add_spirv_shader(vertex "${shader}" "${shader}.spv")
    set(EditorShaders "${EditorShaders}" "${CMAKE_BINARY_DIR}/resources/shaders/${shader}.spv")
endforeach()

if (MSVC)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

set(EditorSources "${EditorSources}" "${EditorShaders}")

add_library(Peeler-Lib ${EditorSources} ${ImGuizmoSources})
add_includes(Peeler-Lib)
add_editor_defines(Peeler-Lib)
target_include_directories(Peeler-Lib PUBLIC ../thirdparty/ImGuizmo)
target_link_libraries(Peeler-Lib PUBLIC Engine-Base)

add_library(Peeler-Renderdoc-Lib ${EditorSources} ${ImGuizmoSources})
add_includes(Peeler-Renderdoc-Lib)
add_editor_defines(Peeler-Renderdoc-Lib)
target_compile_definitions(Peeler-Renderdoc-Lib PRIVATE DEBUG_MARKERS=1)
target_include_directories(Peeler-Renderdoc-Lib PUBLIC ../thirdparty/ImGuizmo)
target_link_libraries(Peeler-Renderdoc-Lib PUBLIC Engine-Base-Renderdoc)

add_library(Peeler-ASAN-Lib ${EditorSources} ${ImGuizmoSources})
make_asan_target(Peeler-ASAN-Lib)
add_includes(Peeler-ASAN-Lib)
add_editor_defines(Peeler-ASAN-Lib)
target_include_directories(Peeler-ASAN-Lib PUBLIC ../thirdparty/ImGuizmo)
target_link_libraries(Peeler-ASAN-Lib PUBLIC Engine-ASAN)

add_executable(Peeler ${EditorRoot}main.cpp)
add_includes(Peeler)
add_editor_defines(Peeler)
target_link_libraries(Peeler PUBLIC Peeler-Lib)

add_engine_precompiled_headers(Peeler-Lib)
add_engine_precompiled_headers(Peeler-Renderdoc-Lib)
add_engine_precompiled_headers(Peeler-ASAN-Lib)
add_engine_precompiled_headers(Peeler)

copy_all_resources() # game resources