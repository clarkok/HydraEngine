set(COMMON_TEST_SRCS
    encoding-test.cpp)

add_executable(common_test ${COMMON_TEST_SRCS})
add_test(common_test common_test)
