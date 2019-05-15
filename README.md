# blzlib - Bluez BLE GATT Library #

`blzlib` is a C library for acessing BLE GATT Services thru Bluez via its D-Bus API on Linux. It can be used as an easy interface to GATT Services and Characteristics without having to worry about the underlying D-Bus API of Bluez.


## Dependencies ##

* [Bluez](http://www.bluez.org/) version 5.49 and higher

* libsystemd: blzlib uses systemds [sd-bus](http://0pointer.net/blog/the-new-sd-bus-api-of-systemd.html) API to interface with D-Bus.

      sudo apt install libsystemd-dev


## Building ##

Currently building with `meson` as well as `cmake` is supported.

Meson:

    meson build
    cd build/
    ninja

CMake:

    mkdir build
    cd build
    cmake ..
    make


## Status ##
    
Supported:
    
  * Discovery / Scanning of remote BLE devices
  * Read GATT Characteristics
  * Notify of GATT Characteristics
  * Write GATT Characteristics
  * Efficient write of GATT Characteristics by file descriptor (write-without-respose)

TODO

  * Create local GATT Services and Characteristics to be accessed by remote devices (Server side)
