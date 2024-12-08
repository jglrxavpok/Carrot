add_executable(pipelinecompiler
        main.cpp
)
add_core_includes(pipelinecompiler)
target_link_libraries(pipelinecompiler PUBLIC CarrotCore shadercompiler-lib)