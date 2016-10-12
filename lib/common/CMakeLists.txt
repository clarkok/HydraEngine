set(HYDRA_COMMON_SRCS
	assert.hpp
	assert.cpp
	encoding.hpp
	encoding.cpp
	common.hpp
	common.cpp)

add_library(common ${HYDRA_COMMON_SRCS})
