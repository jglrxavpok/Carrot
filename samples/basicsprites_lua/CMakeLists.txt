set(SampleRoot "")

set(GameSources
        ${SampleRoot}main.cpp
        )

add_executable(BasicSprites-Lua ${GameSources})
add_includes(BasicSprites-Lua)
target_link_libraries(BasicSprites-Lua PUBLIC Engine-Base)
