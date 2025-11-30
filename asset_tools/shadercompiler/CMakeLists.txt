add_library(shadercompiler-lib STATIC
        FileIncluder.cpp
        GlslCompiler.cpp
        ShaderCompiler.cpp
        SlangCompiler.cpp

        ../../thirdparty/glslang/glslang/ResourceLimits/ResourceLimits.cpp
)
add_core_includes(shadercompiler-lib)
target_link_libraries(shadercompiler-lib PUBLIC glslang glslang::SPIRV CarrotCore slang spirv-reflect-static)

add_executable(shadercompiler main.cpp)
target_link_libraries(shadercompiler PUBLIC shadercompiler-lib)