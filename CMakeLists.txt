cmake_minimum_required(VERSION 3.0.0)
project(sdgatt VERSION 0.1.0)

add_executable(sdgatt
	main.c
	logging.c
	blzlib.c
	blzlib_msgs.c
)

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBSYSTEMD REQUIRED libsystemd)

target_include_directories(sdgatt PRIVATE .
	${LIBSYSTEMD_INCLUDE_DIRS})

target_link_libraries(sdgatt ${LIBSYSTEMD_LIBRARIES})

set(CMAKE_C_FLAGS "-DDEBUG=1")

install(TARGETS sdgatt
	RUNTIME DESTINATION sbin)
