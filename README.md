# blzlib - Bluetooth GATT Library for Linux / Bluez #

`blzlib` is a C library providing an easy interface to acess Bluetooth Low Energy (BLE) Generic Attribute Profile (GATT) Services and Characteristics on Linux. It interfaces with the Bluez Bluetooth daemon on Linux via its D-Bus API. It depends on `libsystemd` (sd-bus) and is licensed under the [LGPLv3](LICENSE.txt).

Currently the following features are supported:

  * Discovery / Scanning of nearby BLE devices
  * Discovery of services and characteristics
  * Read GATT characteristics
  * Notify of GATT characteristics (value change notifications)
  * Write GATT characteristics
  * Efficient write of GATT characteristics by file descriptor (write-without-respose)

## Dependencies ##

* [Bluez](http://www.bluez.org/) version 5.49 and higher

* libsystemd: blzlib uses [systemds](https://www.freedesktop.org/wiki/Software/systemd/) [sd-bus](http://0pointer.net/blog/the-new-sd-bus-api-of-systemd.html) library to access the [D-Bus](https://www.freedesktop.org/wiki/Software/dbus/) API of Bluez.

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


## Examples ##

Here is a simple example how to write a characteristic:

```
blz* blz = blz_init("hci0");
blz_dev* dev = blz_connect(blz, "00:11:22:33:44:55", NULL);
blz_char* ch = blz_get_char_from_uuid(dev, "6e400002-b5a3-f393-e0a9-e50e24dcca9e");
blz_char_write(ch, "test", 4);
```

More real-life examples can be found in the [examples/](examples/) directory.
