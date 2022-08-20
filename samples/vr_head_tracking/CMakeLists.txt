set(SampleRoot "")

set(GameSources
        ${SampleRoot}main.cpp
        )


copy_all_resources()

add_executable(SampleVRHead ${GameSources})
target_link_libraries(SampleVRHead PUBLIC Engine-Base)
