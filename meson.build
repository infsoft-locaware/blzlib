project('blzlib', 'c',
	version: '0.0.2',
	license: 'GPL2')

libsystemd = dependency('libsystemd')

blzlib = both_libraries('blzlib',
	'blzlib.c', 'blzlib_util.c', 'blzlib_msgs.c', 'blzlib_log.c',
	dependencies: libsystemd,
	install: true)

install_headers('blzlib.h', 'blzlib_util.h', 'blzlib_log.h')

pkg_mod = import('pkgconfig')
pkg_mod.generate(blzlib)

executable('blz-nordic-uart',
	'examples/nordic-uart.c',
	link_with: blzlib,
	dependencies: libsystemd)

executable('blz-read-manuf-name',
	'examples/read-manuf-name.c',
	link_with: blzlib)

executable('blz-scan-discover',
	'examples/scan-discover.c',
	link_with: blzlib)
