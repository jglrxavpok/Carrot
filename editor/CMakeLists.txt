set(EditorRoot "src/")

set(EditorSources
        ${EditorRoot}main.cpp
        ${EditorRoot}Peeler.cpp
        )

if (MSVC)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

add_executable(Peeler ${EditorSources})
target_link_libraries(Peeler PUBLIC Engine-Base)

copy_all_resources() # game resources