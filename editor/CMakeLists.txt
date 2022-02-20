set(EditorRoot "src/")

set(EditorSources
        ${EditorRoot}main.cpp
        ${EditorRoot}Peeler.cpp
        ${EditorRoot}Scene.cpp
        ${EditorRoot}GridRenderer.cpp
        ${EditorRoot}ecs/systems/CameraRenderer.cpp
        ${EditorRoot}ecs/systems/CollisionShapeRenderer.cpp
        ${EditorRoot}ecs/systems/LightEditorRenderer.cpp
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
        )

set(EditorVertexShaders
        grid.vertex.glsl
        )

set(EditorShaders "")

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

add_executable(Peeler ${EditorSources} ${ImGuizmoSources})
add_includes(Peeler)
target_include_directories(Peeler PUBLIC ../thirdparty/ImGuizmo)
target_link_libraries(Peeler PUBLIC Engine-Base)

add_executable(Peeler-Renderdoc ${EditorSources} ${ImGuizmoSources})
add_includes(Peeler-Renderdoc)
target_compile_definitions(Peeler-Renderdoc PRIVATE DEBUG_MARKERS=1)
target_include_directories(Peeler-Renderdoc PUBLIC ../thirdparty/ImGuizmo)
target_link_libraries(Peeler-Renderdoc PUBLIC Engine-Base-Renderdoc)

add_executable(Peeler-ASAN ${EditorSources} ${ImGuizmoSources})
target_link_libraries(Peeler-ASAN PUBLIC Engine-ASAN)
target_include_directories(Peeler-ASAN PUBLIC ../thirdparty/ImGuizmo)
make_asan_target(Peeler-ASAN)

copy_all_resources() # game resources