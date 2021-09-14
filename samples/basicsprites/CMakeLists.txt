set(SampleRoot "")

set(GameSources
        ${SampleRoot}main.cpp
        )

add_executable(BasicSprites ${GameSources})
target_link_libraries(BasicSprites PUBLIC Engine-Base)

add_executable(BasicSprites-Renderdoc ${GameSources})
target_link_libraries(BasicSprites-Renderdoc PUBLIC Engine-Base-Renderdoc)
target_compile_definitions(BasicSprites-Renderdoc PRIVATE DEBUG_MARKERS=1)
