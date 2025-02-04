add_executable(sceneconverter
        sceneconverter.cpp
)
add_core_includes(sceneconverter)
target_link_libraries(sceneconverter PUBLIC CarrotCore)