set(EditorRoot "src/")

set(EditorSources
        ${EditorRoot}main.cpp
        ${EditorRoot}Peeler.cpp
        ${EditorRoot}Scene.cpp
        )

set(ImGuizmoSources
        ../thirdparty/ImGuizmo/GraphEditor.cpp
        ../thirdparty/ImGuizmo/ImCurveEdit.cpp
        ../thirdparty/ImGuizmo/ImGradient.cpp
        ../thirdparty/ImGuizmo/ImGuizmo.cpp
        ../thirdparty/ImGuizmo/ImSequencer.cpp
        )

if (MSVC)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

add_executable(Peeler ${EditorSources} ${ImGuizmoSources})
add_includes(Peeler)
target_include_directories(Peeler PUBLIC ../thirdparty/ImGuizmo)
target_link_libraries(Peeler PUBLIC Engine-Base)

add_executable(Peeler-Tracy ${EditorSources} ${ImGuizmoSources})
add_includes(Peeler-Tracy)
target_include_directories(Peeler-Tracy PUBLIC ../thirdparty/ImGuizmo)
target_link_libraries(Peeler-Tracy PUBLIC Engine-Tracy)
make_tracy_target(Peeler-Tracy)

add_executable(Peeler-Renderdoc ${EditorSources} ${ImGuizmoSources})
add_includes(Peeler-Renderdoc)
target_include_directories(Peeler-Renderdoc PUBLIC ../thirdparty/ImGuizmo)
target_link_libraries(Peeler-Renderdoc PUBLIC Engine-Base-Renderdoc)

copy_all_resources() # game resources