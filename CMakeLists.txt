cmake_minimum_required(VERSION 3.0.0)
project(sdgatt VERSION 0.1.0)

add_library(blzlib SHARED
	blzlib.c
	blzlib_msgs.c
	blzlib_util.c
	blzlib_log.c)

add_executable(blz-nordic-uart
	examples/nordic-uart.c)

add_executable(blz-read-manuf-name
	examples/read-manuf-name.c)

add_executable(blz-scan-discover
	examples/scan-discover.c)

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBSYSTEMD REQUIRED libsystemd)

# Add the build directory for the examples to link the currently built library
link_directories(${PROJECT_BINARY_DIR})

target_include_directories(blz-nordic-uart PRIVATE .)
target_include_directories(blz-read-manuf-name PRIVATE .)
target_include_directories(blz-scan-discover PRIVATE .)

target_link_libraries(blz-nordic-uart blzlib ${LIBSYSTEMD_LIBRARIES})
target_link_libraries(blz-read-manuf-name blzlib ${LIBSYSTEMD_LIBRARIES})
target_link_libraries(blz-scan-discover blzlib ${LIBSYSTEMD_LIBRARIES})

set(CMAKE_C_FLAGS "-DDEBUG=1")

install(FILES blzlib.h blzlib_util.h blzlib_log.h
	DESTINATION include
)

configure_file(blzlib.pc.in ${PROJECT_BINARY_DIR}/blzlib.pc @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/blzlib.pc DESTINATION lib/pkgconfig)

install(TARGETS blzlib blz-nordic-uart blz-nordic-uart blz-scan-discover
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin)
