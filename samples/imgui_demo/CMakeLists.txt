set(SampleRoot "")

set(GameSources
        ${SampleRoot}main.cpp
        )

add_executable(ImGuiDemoOnCarrot ${GameSources})
target_link_libraries(ImGuiDemoOnCarrot PUBLIC Engine-Base)
