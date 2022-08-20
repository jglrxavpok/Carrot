set(SampleRoot "")

set(GameSources
        ${SampleRoot}main.cpp
        )


copy_all_resources()

add_executable(SampleVRHand ${GameSources})
target_link_libraries(SampleVRHand PUBLIC Engine-Base)
