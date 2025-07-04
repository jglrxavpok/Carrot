set(SampleRoot "")

set(GameSources
        ${SampleRoot}main.cpp
        )


copy_all_resources()

add_executable(SampleManualSkeleton ${GameSources})
add_includes(SampleManualSkeleton)
add_engine_precompiled_headers(SampleManualSkeleton)
target_link_libraries(SampleManualSkeleton PUBLIC Engine-Base)
