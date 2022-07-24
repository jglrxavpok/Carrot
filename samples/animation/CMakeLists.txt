set(SampleRoot "")

set(GameSources
        ${SampleRoot}main.cpp
        )

add_executable(SampleAnimation ${GameSources})
target_link_libraries(SampleAnimation PUBLIC Engine-Base)

add_executable(SampleAnimation-Renderdoc ${GameSources})
target_link_libraries(SampleAnimation-Renderdoc PUBLIC Engine-Base-Renderdoc)
target_compile_definitions(SampleAnimation-Renderdoc PRIVATE DEBUG_MARKERS=1)
