set(SampleRoot "")

set(GameSources
        ${SampleRoot}main.cpp

        ${SampleRoot}Game.cpp
        )
add_library(SampleGame ${GameSources})
target_link_libraries(SampleGame PUBLIC Engine-Base)

add_executable(SampleGame-Basic ${ProjectRoot}engine/null.cpp)
target_link_libraries(SampleGame-Basic PUBLIC SampleGame)

add_executable(SampleGame-ASAN ${GameSources})
target_link_libraries(SampleGame-ASAN PUBLIC Engine-ASAN)
make_asan_target(SampleGame-ASAN)