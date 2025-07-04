set(SampleRoot "")

set(GameSources
        ${SampleRoot}main.cpp
        )

add_executable(SampleAnimation ${GameSources})
add_includes(SampleAnimation)
add_engine_precompiled_headers(SampleAnimation)
target_link_libraries(SampleAnimation PUBLIC Engine-Base)
