# CycloneDDS examples for Zephyr RTOS

## Purpose
This directory contains some proof-of-concept applications that show how to build and use CycloneDDS on [Zephyr RTOS](https://www.zephyrproject.org) for a NXP X-S32Z27X-DC board.


Getting Started with Zephyr information can be found [here](https://docs.zephyrproject.org/3.3.0/develop/getting_started/index.html)


Documentation for the NXP X-S32Z27X-DC board can be found [here](https://docs.zephyrproject.org/3.3.0/boards/arm/s32z270dc2_r52/doc/index.html)

## Usage:
This directory can used like any generic Zephyr application.
The `CMakeLists.txt` is currently set up to build CycloneDDS examples and ddsperf but can be easily modified or extended for other application code.

The copy_examples.sh script can be used (manually) to update the code from CycloneDDS examples and run idlc to generate types.

For example, to build Roundtrip Ping for the `s32z270dc2_rtu0_r52` target:
```
$ west build -b s32z270dc2_rtu0_r52 . -- -DBUILD_ROUNDTRIP_PING=1
```
To build for qemu_x86, with ethernet support:
```
$ west build -b qemu_x86 . -- -DOVERLAY_CONFIG=overlay-e1000.conf -DBUILD_ROUNDTRIP_PING=1
```
Command-line parameters for the example can be modified in `src/rountrip_main.c`


The CycloneDDS configuration in `config.xml` is automatically converted to a char array and available as environment variable to support the default behaviour of retrieving config from CYCLONEDDS_URI.
Alternatively, [dds_create_domain_with_rawconfig](https://cyclonedds.io/docs/cyclonedds/latest/api/domain.html?#c.dds_create_domain_with_rawconfig) can be used without XML configuration data.
