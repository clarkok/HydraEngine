set(RUNTIME_TEST_SRCS
    value-test.cpp
    ../main.cpp)

add_executable(runtime_test ${RUNTIME_TEST_SRCS})

add_test(runtime_value_test runtime_test [Value])
