set(SampleRoot "")

set(GameSources
        ${SampleRoot}HelloWorld.cpp
        )
add_executable(ReactPhysics3D-HelloWorld ${GameSources})
target_link_libraries(ReactPhysics3D-HelloWorld PUBLIC Engine-Base)
