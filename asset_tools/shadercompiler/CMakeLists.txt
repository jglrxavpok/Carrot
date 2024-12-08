add_library(shadercompiler-lib
        FileIncluder.cpp
        ShaderCompiler.cpp

        ../../thirdparty/glslang/glslang/ResourceLimits/ResourceLimits.cpp
)
add_core_includes(shadercompiler-lib)
target_link_libraries(shadercompiler-lib PUBLIC glslang SPIRV CarrotCore)

add_executable(shadercompiler main.cpp)
target_link_libraries(shadercompiler PUBLIC shadercompiler-lib)