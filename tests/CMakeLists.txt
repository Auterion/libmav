include(CTest)

add_executable(tests
        test_main.cpp
        MessageSet.cpp
        Message.cpp
        MessageDefinition.cpp
        Network.cpp
        MessageFieldIterator.cpp
        UDP.cpp
        TCP.cpp)

add_test(NAME tests COMMAND tests)

target_compile_options(tests PRIVATE -O0 -g --coverage)
target_link_options(tests PRIVATE --coverage)
target_include_directories(tests PRIVATE ../include)
