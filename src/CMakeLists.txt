set(HYDRA_SRCS main.cpp)

add_executable(hydra ${HYDRA_SRCS})
target_link_libraries(hydra
	common
	runtime
	parser)
