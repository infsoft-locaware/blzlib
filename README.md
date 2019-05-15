# blzlib - Bluez BLE GATT Library #

`blzlib` is a library for acessing BLE GATT Services thru Bluez via its D-Bus API on Linux. It can be used as an easy interface to GATT Services and Characteristics without having to worry about the underlying D-Bus API of Bluez.


## Dependencies ##

* Bluez version 5.49 and higher

* libsystemd: blzlib uses systemds excellent `sd-bus` API to interface with D-Bus.

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
    
Currently supported:
    
  * Discovery / Scanning of remote BLE devices
  * Reading GATT Characteristics
  * Notify of GATT Characteristics
  * Writing GATT Characteristic
  * Efficient writing of GATT Characteristics by file descriptor (write-without-respose)

TODO

  * Extend for creating local GATT Services and Characteristics to be accessed by remote devices
