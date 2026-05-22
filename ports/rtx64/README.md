# CycloneDDS for RTX64

## Build

### Requirements

RTX64 SDK v4.5.5 or later

### Build

Open a Visual Studio Developer Command Prompt and navigate to the root of the repository.

**Pass 1: make a Windows build**

```
mkdir build_host
cd build_host
cmake -DCMAKE_INSTALL_PREFIX=<windows-install-location> ..
cmake --build . --config Release
cmake --build . --config Release --target install
```

Note: the goal of Pass 1 is to produce the ``idlc.exe`` tool, which is needed by Pass 2.

**Pass 2: make the RTX64 build**

```
mkdir build_target
cd build_target
cmake -DCMAKE_TOOLCHAIN_FILE=..\ports\rtx64\msvc-rtx64-toolchain.cmake -DBUILD_DDSPERF=[ON|OFF] -DBUILD_EXAMPLES=[ON|OFF] -DHOST_INSTALL_PREFIX=<windows-install-location> -DCMAKE_INSTALL_PREFIX=<rtx64-install-location> ..
cmake --build . --config RtssRelease
cmake --build . --config RtssRelease --target install
```

### Test the installation

Make sure you have a NIC configured with TCP/IP support.

If needed, create a configuration file (see https://cyclonedds.io/docs/cyclonedds/latest/config/index.html) and create a system environment variable named ``CYCLONEDDS_URI`` that points to it. A configuration file might be useful if you have multiple NICs configured and you want to specify which one Cyclone DDS shall use.

Start the RTX64 runtime, the NAL and the TCP/IP Stack.

Go to ``<rtx64-install-location>\bin`` and do ``rtssrun ddsperf sanity``.

On another machine connected to the same network, run ``ddsperf pong``.

Note: the other machine may be running RTX64, Windows, Linux, or any other system supported by Cyclone DDS.


## Known limitations

- Security feature is not supported.
- Raw Ethernet transport is not supported.
- Shared Memory (psmx_iox) transport is not supported.
- Thread and Network statistics are not supported.
- "DontRoute" configuration option is not supported (MUST be set to "false").
- "EnableMulticastLoopback" configuration option is not supported (MUST be set to "false").
- "ExtendedPacketInfo" configuration option is not supported (MUST be set to "false").
- "MaxMessageSize" configuration option MUST be lower than or equal to 1500.

