add_executable(shadercompiler
        shadercompiler/FileIncluder.cpp
        shadercompiler/main.cpp

        ../thirdparty/glslang/StandAlone/ResourceLimits.cpp
)
target_link_libraries(shadercompiler PUBLIC glslang SPIRV)