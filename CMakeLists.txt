cmake_minimum_required(VERSION 3.0.0)
project(sdgatt VERSION 0.1.0)

add_library(blz SHARED
	blzlib.c
	blzlib_msgs.c
	blzlib_util.c
	blzlib_log.c)

add_executable(nordic_uart
	examples/nordic_uart.c)

add_executable(read_manuf_name
	examples/read_manuf_name.c)

add_executable(scan
	examples/scan.c)

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBSYSTEMD REQUIRED libsystemd)

target_include_directories(nordic_uart PRIVATE .)
target_include_directories(read_manuf_name PRIVATE .)
target_include_directories(scan PRIVATE .)

target_link_libraries(nordic_uart blz ${LIBSYSTEMD_LIBRARIES})
target_link_libraries(read_manuf_name blz ${LIBSYSTEMD_LIBRARIES})
target_link_libraries(scan blz ${LIBSYSTEMD_LIBRARIES})

set(CMAKE_C_FLAGS "-DDEBUG=1")

install(FILES blzlib.h blzlib_util.h
	DESTINATION include
)

configure_file(blzlib.pc.in ${PROJECT_BINARY_DIR}/blzlib.pc @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/blzlib.pc DESTINATION lib/pkgconfig)

install(TARGETS blz nordic_uart read_manuf_name
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin)
