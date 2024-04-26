# CycloneDDS examples for Zephyr RTOS

## Purpose
This directory contains some proof-of-concept applications that show how to build and use CycloneDDS on [Zephyr RTOS](https://www.zephyrproject.org) for a NXP X-S32Z27X-DC board.


Getting Started with Zephyr information can be found [here](https://docs.zephyrproject.org/3.6.0/develop/getting_started/index.html)
Documentation for the NXP X-S32Z27X-DC board can be found [here](https://docs.zephyrproject.org/3.6.0/boards/arm/s32z270dc2_r52/doc/index.html)

## Usage
The applications can be treated similarly to other Zephyr sample applications.
The `CMakeLists.txt` is currently set up to build some of the CycloneDDS examples or ddsperf but can be easily modified or extended for other application code.

:warning: While CycloneDDS can be built in-tree, it does not support multiple different builds. Therefore the CycloneDDS source directory needs to be cleaned in order to build for Zephyr and must not contain an in-tree build for eg. the host system. Alternatively, this directory with Zephyr examples can be copied outside the CycloneDDS source directory but that requires updating the `ExternalProject_Add` directive in `CMakeLists.txt` to point to the CycloneDDS source directory.

:warning: When building outside the Zephyr tree, `ZEPHYR_BASE` variable must be set. See [Application Development](https://docs.zephyrproject.org/3.6.0/develop/application/index.html) for more info.

:warning: A static IPv4/IPv6 address is defined in `prj.conf`. When running between two Zephyr nodes it is suggested to copy `prj.conf` to eg. `prj-host1.conf` and `prj-host2.conf`, updating the address as required and building with `-DCONF=prj-host1.conf` or `-DCONF=prj-host2.conf` respectively, as described in more detail [here](https://docs.zephyrproject.org/3.6.0/samples/net/eth_native_posix/README.html). A different approach is to enable DHCP client support in Zephyr (see [example](https://docs.zephyrproject.org/3.6.0/samples/net/dhcpv4_client/README.html)).

The `copy_examples.sh` script can be used to (manually) update the code from CycloneDDS examples and run `idlc` to generate types.

For example, to build Roundtrip Ping for the NXP S32Z270-DC2 board:
```
$ west build -b s32z270dc2_rtu0_r52 . -- -DBUILD_ROUNDTRIP_PING=1
```
To build for qemu_x86, with ethernet support:
```
$ west build -b qemu_x86 . -- -DOVERLAY_CONFIG=overlay-e1000.conf -DBUILD_ROUNDTRIP_PING=1
```
:warning: In the current Zephyr v3.7.0 (draft) branch, board naming has been refactored and you should use `s32z2xxdc2/s32z270/rtu0`.

Command-line parameters for the example can be modified in `src/rountrip_main.c`

The CycloneDDS configuration in `config.xml` is automatically converted to a char array and available as environment variable to support the default behaviour of retrieving config from `CYCLONEDDS_URI`.
Alternatively, [dds_create_domain_with_rawconfig](https://cyclonedds.io/docs/cyclonedds/latest/api/domain.html?#c.dds_create_domain_with_rawconfig) can be used without XML configuration data.

## Zephyr versions
At the time of writing, CycloneDDS has been tested on Zephyr [v3.6.0](https://github.com/zephyrproject-rtos/zephyr/releases/tag/v3.6.0)
To use CycloneDDS with the current (draft) V3.7.0 release, please replace `src/ddsrt/src/ifaddrs/zephyr/ifaddrs.c` with `ifaddrs-v3.7pre.c` which is compatible with the updated (IPv4) Networking APIs in Zephyr (though this code is in flux so YMMV).
