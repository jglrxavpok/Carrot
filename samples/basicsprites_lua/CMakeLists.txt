set(SampleRoot "")

set(GameSources
        ${SampleRoot}main.cpp
        )

add_executable(BasicSprites-Lua ${GameSources})
target_link_libraries(BasicSprites-Lua PUBLIC Engine-Base)
