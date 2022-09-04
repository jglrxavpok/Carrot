set(SampleRoot "")

set(GameSources
        ${SampleRoot}main.cpp
        )


copy_all_resources()

add_executable(SampleXRActions ${GameSources})
target_link_libraries(SampleXRActions PUBLIC Engine-Base)
