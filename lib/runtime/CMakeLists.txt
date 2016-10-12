set(HYDRA_RUNTIME_SRCS
	runtime.cpp
	heap.hpp
	heap.cpp
	)

add_library(runtime ${HYDRA_RUNTIME_SRCS})
