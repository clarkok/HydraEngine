set(HYDRA_COMMON_SRCS
	exception.hpp
	assert.hpp
	encoding.hpp
	encoding.cpp
	common.hpp
	common.cpp)

add_library(common ${HYDRA_COMMON_SRCS})
