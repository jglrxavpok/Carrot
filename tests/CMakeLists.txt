set(AllTests "")

function(make_test Test)
    list(APPEND AllTests ${Test})
    add_executable("Carrot-Test${Test}" ${Test}.cpp)
    target_link_libraries("Carrot-Test${Test}" PUBLIC Engine-Base)
endfunction()

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

make_test(Audio)
make_test(Resources)
make_test(Network-Client)
make_test(Network-Server)
make_test(Lua)
make_test(GeneralMaterials)

enable_testing()

add_executable(
        Engine-Tests
        Coroutines.cpp
        FileWatching.cpp
)
target_link_libraries(
        Engine-Tests
        PUBLIC Engine-Base
        gtest_main
)

include(GoogleTest)
gtest_discover_tests(Engine-Tests)

copy_all_resources()