set(SampleRoot "")

set(GameSources
        ${SampleRoot}main.cpp
        )


copy_all_resources()

add_executable(SampleManualSkeleton ${GameSources})
add_includes(SampleManualSkeleton)
add_engine_precompiled_headers(SampleManualSkeleton)
target_link_libraries(SampleManualSkeleton PUBLIC Engine-Base)

add_executable(SampleManualSkeleton-Renderdoc ${GameSources})
target_link_libraries(SampleManualSkeleton-Renderdoc PUBLIC Engine-Base-Renderdoc)
target_compile_definitions(SampleManualSkeleton-Renderdoc PRIVATE DEBUG_MARKERS=1)
