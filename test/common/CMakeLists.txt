set(COMMON_TEST_SRCS
    ptr-test.cpp)

add_executable(common_test ${COMMON_TEST_SRCS})
add_test(Ptr common_test [Ptr])