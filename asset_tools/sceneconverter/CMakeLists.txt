add_library(sceneconverter-lib
        sceneconverter.cpp
)
add_executable(sceneconverter
        main.cpp
)
add_core_includes(sceneconverter-lib)
target_link_libraries(sceneconverter-lib PUBLIC CarrotCore)
target_link_libraries(sceneconverter PUBLIC sceneconverter-lib)