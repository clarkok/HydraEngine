set(COMMON_TEST_SRCS
    encoding-test.cpp
	assert-test.cpp
	../main.cpp)

add_executable(common_test ${COMMON_TEST_SRCS})

add_test(common_encoding_test common_test [Encoding])
add_test(common_assert_test common_test [Assert])
