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

# Coverage and GCC/Clang-specific warning flags are not understood by MSVC.
if(NOT MSVC)
    target_compile_options(tests PRIVATE -O0 -g --coverage -Wall -Wextra -pedantic)
    target_link_options(tests PRIVATE --coverage)
endif()
target_include_directories(tests PRIVATE ../include)
target_link_libraries(tests PRIVATE Threads::Threads)
