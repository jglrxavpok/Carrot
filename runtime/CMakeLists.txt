set(RuntimeSources
    "src/Runtime.cpp"
)


if (MSVC)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

add_executable(Runtime ${RuntimeSources})
add_engine_precompiled_headers(Runtime)
add_includes(Runtime)
target_link_libraries(Runtime PUBLIC Engine-Base)
