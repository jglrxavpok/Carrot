add_executable(shadercompiler
        FileIncluder.cpp
        main.cpp

        ../../thirdparty/glslang/glslang/ResourceLimits/ResourceLimits.cpp
)
add_core_includes(shadercompiler)
target_link_libraries(shadercompiler PUBLIC glslang SPIRV CarrotCore)