# FreeRTOS

[FreeRTOS][1] is real-time operating system kernel for embedded devices. Think
of it as a thread library rather than a general purpose operating system like
Linux or Microsoft Windows. Out-of-the-box, FreeRTOS provides support for
tasks (threads), mutexes, semaphores and software times. Third-party modules
are available to add features. e.g. [lwIP][2] can be used to add networking.

> FreeRTOS+lwIP is currently supported by Eclipse Cyclone DDS. Support for other
> network stacks, e.g. [FreeRTOS+TCP][3], may be added in the future.

> Eclipse Cyclone DDS does not make use of [FreeRTOS+POSIX][4] because it was
> not available at the time. Future versions of Eclipse Cyclone DDS may or may
> not require FreeRTOS+POSIX for compatibility when it becomes stable.

[1]: https://www.freertos.org/
[2]: https://savannah.nongnu.org/projects/lwip/
[3]: https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/index.html
[4]: https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_POSIX/index.html


## Target

FreeRTOS provides an operating system kernel. Batteries are not included. i.e.
no C library or device drivers. Third-party distributions, known as board
support packages (BSP), for various (hardware) platforms are available though.

Board support packages, apart from FreeRTOS, contain:

* C library. Often ships with the compiler toolchain, e.g.
  [IAR Embedded Workbench][5] includes the DLIB runtime, but open source
  libraries can also be used. e.g. The [Xilinx Software Development Kit][6]
  includes newlib.
* Device drivers. Generally available from the hardware vendor, e.g. NXP or
  Xilinx. Device drivers for extra components, like a real-time clock, must
  also be included in the board support package.

[5]: https://www.iar.com/iar-embedded-workbench/
[6]: https://www.xilinx.com/products/design-tools/embedded-software/sdk.html

A board support package is linked with the application by the toolchain to
generate a binary that can be flashed to the target.


### Requirements

Eclipse Cyclone DDS requires certain compile-time options to be enabled in
FreeRTOS (`FreeRTOSConfig.h`) and lwIP (`lwipopts.h`) for correct operation.
The compiler will croak when a required compile-time option is not enabled.

Apart from the aforementioned compile-time options, the target and toolchain
must provide the following.
* Support for thread-local storage (TLS) from the compiler and linker.
* Berkeley socket API compatible socket interface.
* Real-time clock (RTC). A high-precision real-time clock is preferred, but
  the monotonic clock can be combined with an offset obtained from e.g. the
  network if the target lacks an actual real-time clock. A proper
  `clock_gettime` implementation is required to retrieve the wall clock time.


### Thread-local storage

FreeRTOS tasks are not threads and compiler supported thread-local storage
(tls) might not work as desired/expected under FreeRTOS on embedded targets.
i.e. the address of a given variable defined with *__thread* may be the same
for different tasks.

The compiler generates code to retrieve a unique address per thread when it
encounters a variable defined with *__thread*. What code it generates depends
on the compiler and the target it generates the code for. e.g. `iccarm.exe`
that comes with IAR Embedded Workbench requires `__aeabi_read_tp` to be
implemented and `mb-gcc` that comes with the Xilinx SDK requires
`__tls_get_addr` to be implemented.

The implementation for each of these functions is more-or-less the same.
Generally speaking they require the number of bytes to allocate, call
`pvTaskGetThreadLocalStoragePointer` and return the address of the memory
block to the caller.


## Simulator

FreeRTOS ports for Windows and POSIX exist to test compatibility. How to
cross-compile Eclipse Cyclone DDS for the [FreeRTOS Windows Port][7] or
the unofficial [FreeRTOS POSIX Port][8] can be found in the msvc and
[posix](/ports/freertos-posix) port directories.

[7]: https://www.freertos.org/FreeRTOS-Windows-Simulator-Emulator-for-Visual-Studio-and-Eclipse-MingW.html
[8]: https://github.com/shlinym/FreeRTOS-Sim.git


## Known Limitations

Triggering the socket waitset is not (yet) implemented for FreeRTOS+lwIP. This
introduces issues in scenarios where it is required.

 * Receive threads require a trigger to shutdown or a thread may block
   indefinitely if no packet arrives from the network.
 * Sockets are created dynamically if ManySocketsMode is used and a participant
   is created, or TCP is used. A trigger is issued after the sockets are added
   to the set if I/O multiplexing logic does not automatically wait for data
   on the newly created sockets as well.

