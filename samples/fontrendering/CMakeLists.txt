set(SampleRoot "")

set(GameSources
        ${SampleRoot}main.cpp
        )

add_executable(FontRenderingSample ${GameSources})
add_includes(FontRenderingSample)
target_link_libraries(FontRenderingSample PUBLIC Engine-Base)
