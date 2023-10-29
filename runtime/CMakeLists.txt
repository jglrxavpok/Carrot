set(RuntimeSources
    "src/Runtime.cpp"
)

add_executable(Runtime ${RuntimeSources})
add_engine_precompiled_headers(Runtime)
add_includes(Runtime)
target_link_libraries(Runtime PUBLIC Engine-Base)
