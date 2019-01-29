project('blzlib', 'c',
	version: '0.0.1',
	license: 'GPL2')

libsystemd = dependency('libsystemd')

blzlib = both_libraries('blzlib',
	'blzlib.c', 'blzlib_util.c', 'blzlib_msgs.c', 'blzlib_log.c',
	dependencies: libsystemd,
	install: true)

install_headers('blzlib.h', 'blzlib_util.h')

pkg_mod = import('pkgconfig')
pkg_mod.generate(blzlib)

executable('nordic_uart', 
	'examples/nordic_uart.c',
	link_with: blzlib,
	dependencies: libsystemd)

executable('read_manuf_name',
	'examples/read_manuf_name.c',
	link_with: blzlib)
