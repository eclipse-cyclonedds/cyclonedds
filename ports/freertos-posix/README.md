# FreeRTOS POSIX Port

FreeRTOS is a supported platform for Eclipse Cyclone DDS. This document
explains how to build and run Eclipse Cyclone DDS on the FreeRTOS POSIX Port.
For basic information, see: [freertos.md](/docs/dev/freertos.md).

As the steps for building and running on the FreeRTOS POSIX Port are largely
the same as building and running on an actual embedded target, this document
should provide an excellent starting point for users. Apart from that, the
simulator can be used to verify commits do not break FreeRTOS compatibility.

> lwIP can also be used in combination with both UNIX and Windows targets, but
> the simulator does not yet have integration. Once integration between both
> is figured out, this document should be updated accordingly.


## Build and install the simulator

The FreeRTOS POSIX Port is not maintained by the FreeRTOS project. Various
projects are maintained across the internet. At the time of writing, the
version maintained by [Shilin][1] seemed the best as the version maintained by
[megakilo][2] was archived.

[1]: https://github.com/shlinym/FreeRTOS-Sim.git
[2]: https://github.com/megakilo/FreeRTOS-Sim

> A [FreeRTOS Linux Port][3] is in the works. Once it becomes stable, please
> update this document accordingly.

[3]: https://sourceforge.net/p/freertos/discussion/382005/thread/f28af711/


1. Clone the repository. The `CMakeLists.txt` in this directory assumes the
   sources are available `./FreeRTOS-Sim` by default, but a different location
   can be specified using the CMake option `FREERTOS_SOURCE_DIR`.
 ```
git clone https://github.com/shlinym/FreeRTOS-Sim.git
```

2. Specify an installation prefix and build the simulator like any other
   CMake project.
 ```
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=$(pwd)/install ..
cmake --build . --target install
```

> A CMake toolchain file is generated and installed into a `share` directory
> located under CMAKE\_INSTALL\_PREFIX/FreeRTOS-Sim. The compiler that CMake
> discovers and uses to build the simulator is exported in the toolchain file
> and will also be used to build Eclipse Cyclone DDS in the following steps.


## Build Eclipse Cyclone DDS for the simulator

1. Change to the root of the repository and install the dependencies. 
 ```
mkdir build
cd build
conan install -s arch=x86 ..
```

> For actual cross-compilation environments the instructions above will not
> install the correct packages. Even when e.g. Clang instead of GCC was used
> to build the simulator, the mismatch between Conan and CMake will break the
> build. To install the correct packages for the target, specify the required
> settings e.g. when the simulator was built using Clang 7.0, use
> `conan install -s arch=x86 -s compiler=clang -s compiler.version=7.0 ..`.
> If packages are not yet available for the target, as is usually the case
> with actual embedded targets, export the path to the toolchain file in the
> `CONAN_CMAKE_TOOLCHAIN_FILE` environment variable and add the `-b` flag to
> build the packages.

2. Build Eclipse Cyclone DDS.
 ```
$ cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain/file -DWITH_FREERTOS=on ../src
```

> Examples (and tests) can be executed like usual. The simulator provides a
> *loader* that initializes the hardware (not used on non-embedded targets),
> starts the scheduler and loads the application.

