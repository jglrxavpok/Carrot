set(AllTests "")

function(make_test Test)
    list(APPEND AllTests ${Test})
    add_executable("Carrot-Test${Test}" ${Test}.cpp)
    target_link_libraries("Carrot-Test${Test}" PUBLIC Engine-Base)
endfunction()

make_test(Audio)
make_test(Resources)
make_test(Network-Client)
make_test(Network-Server)

list(JOIN AllTests " " AllTestsStr)
message("All tests is: " ${AllTestsStr})