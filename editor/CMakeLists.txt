set(EditorRoot "src/")

set(EditorSources
        ${EditorRoot}main.cpp
        ${EditorRoot}Peeler.cpp
        )

add_executable(Peeler ${EditorSources})
target_link_libraries(Peeler PUBLIC Engine-Base)

copy_all_resources() # game resources