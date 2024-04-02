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

target_compile_options(tests PRIVATE -O0 -g --coverage -Wall -Wextra -pedantic)
target_link_options(tests PRIVATE --coverage)
target_include_directories(tests PRIVATE ../include)
target_link_libraries(tests PRIVATE Threads::Threads)

if(WIN32)
    message(STATUS "Building for Windows")
    add_compile_definitions(WINVER=0x0601 _WIN32_WINNT=0x0601) # require at least Windows 7
    target_link_libraries(tests PRIVATE wsock32 ws2_32)
endif()
