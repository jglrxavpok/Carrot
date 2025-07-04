set(SampleRoot "")

set(GameSources
        ${SampleRoot}main.cpp
        )

add_executable(BasicSprites ${GameSources})
add_includes(BasicSprites)
target_link_libraries(BasicSprites PUBLIC Engine-Base)
