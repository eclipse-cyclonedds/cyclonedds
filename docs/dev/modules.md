# Eclipse Cyclone DDS Module Layout

Cyclone DDS is made up of multiple modules, each of which provides a certain
set of functionality, either private, public or a combination therof. Since
Cyclone DDS is a middleware product, the api is of course the most visible
interface. Cyclone DDS uses the *dds* (not followed by an underscore) prefix
to avoid name collisions with other code.

The fact that Cyclone DDS is made up of multiple modules is largely historic,
but does offer a neat way to separate features logically.

     |-------------|
     |             |  DDS is not a target, it is the product, the sum of the
     |     DDS     |  targets that together form Cyclone DDS. i.e. the stable
     |             |  api prefixed with dds\_ and the libddsc.so library.
     |---|---------|
     |   |         |  ddsc implements most of dds\_ api. A modern,
     |   |  ddsc   |  user-friendly implementation of the DDS specification.
     |   |         |
     |   |---------|
     |   |         |  ddsi, as the name suggests, is an implementation of the
     |   |  ddsi   |  RTPS-DDSI specification.
     |   |         |
     |   |---------|
     |             |  ddsrt offers target agnostic implementations of
     |      ddsrt  |  functionality required by the ddsc and ddsi targets, but
     |             |  also exports a subset of the dds\_ api directly. e.g.
     |-------------|  dds_time_t and functions to read the current time from
                      the target are implemented here.

> The need for a separate utility module (util) has disappeared with the
> restructuring of the runtime module. The two will be merged in the not too
> distant future.

All modules are exported seperately, for convenience. e.g. the *ddsrt* module
offers target agnostic interfaces to create and manage threads and
synchronization primitives, retrieve resource usage, system time, etc.
However, all symbols not referenced by including *dds.h* or prefixed with
*dds_* are considered internal and there are no guarantees with regard to api
stability and backwards compatibility. That being said, they are not expected
to change frequently. Module specific headers are located in the respective
directory under `INSTALL_PREFIX/include/cyclonedds`.


## DDS Runtime (ddsrt)
The main purpose of the runtime module is to allow modules stacked on top of
it, e.g. ddsi and dds, to be target agnostic. Meaning that, it ensures that
features required by other modules can be used in the same way across supported
targets. The runtime module will NOT try to mimic or stub features that it can
simply cannot offer on a given target. For features that cannot be implemented
on all targets, a feature macro will be introduced that other modules can use
to test for availability. e.g. *DDSRT_HAVE_IPV6* can be used to determine if
the target supports IPv6 addresses.


### Feature discovery
Discovery of target features at compile time is lagely dynamic. Various target
specific predefined macros determine if a feature is supported and which
implementation is built. This is on purpose, to avoid a target specific
include directory and an abundance of configuration header files and works
well for most use cases. Of course, there are exceptions where the preprocessor
requires some hints to make the right the descision. e.g. when the lwIP TCP/IP
stack should be used as opposed to the native stack. The build system is
responsible for the availability of the proper macros at compile time.

Feature implementations are often tied directly to the operating system for
general purpose operating systems. This level of abstraction is not good
enough for embedded targets though. Whether a feature is available or not
depends on (a combination) of the following.

1. Operating system. e.g. Linux, Windows, FreeRTOS.
2. Compiler. e.g. GCC, Clang, MSVC, IAR.
3. Architecture. e.g. i386, amd64, ARM.
4. C library. e.g. glibc (GNU), dlib (IAR).

#### Atomic operations
Support for atomic operations is determined by the target architecture. Most
compilers (at least GCC, Clang, Microsoft Visual Studio and Solaris Studio)
offer atomic builtins, but if support is unavailable, fall back on the
target architecture specific implementation.

#### Network stack
General purpose operating systems like Microsoft Windows and Linux come with
a network stack, as does VxWorks. FreeRTOS, however, does not and requires a
seperate TCP/IP stack, which is often part of the Board Support Package (BSP).
But separate stacks can be used on Microsoft Windows and Linux too. e.g. the
network stack in Tizen RT is based on lwIP, but the platform uses the Linux
kernel. Wheter or not lwIP must be used cannot be determined automatically and
the build system must hint which implementation is to be used.


### Structure
The runtime module uses a very specific directory structure to allow for
feature-based implementations and sane fallback defaults.

#### Header files
The include directory must provide a header file per feature. e.g.
`cyclonedds/ddsrt/sync.h` is used for synchronisation primitives. If there are
only minor differences between targets, everything is contained within
that file. If not, as is the case with `cyclonedds/ddsrt/types.h`, a header file per
target is a better choice.

Private headers may also be required to share type definitions between target
implementations that do not need to be public. These are located in a feature
specific include directory with the sources.

    ddsrt
     |- include
     |   \- cyclonedds
     |       \- ddsrt
     |           |- atomics
     |           |   |- arm.h
     |           |   |- gcc.h
     |           |   |- msvc.h
     |           |   \- sun.h
     |           |- atomics.h
     |           |- time.h
     |           |- threads
     |           |   |- posix.h
     |           |   \- windows.h
     |           \- threads.h
     |
     \- src
         \- threads
             \- include
                 \- cyclonedds
                     \- ddsrt
                         \- threads_priv.h

> Which target specific header file is included is determined by the top-level 
> header file, not the build system. However, which files are exported 
> automatically is determined by the build system.

#### Source files
Source files are grouped per feature too, but here the build system determines
what is compiled and what is not. By default the build system looks for a
directory with the system name, e.g. windows or linux, but it is possible to
overwrite it from a feature test. This allows for a non-default target to be
used as would be the case with e.g. lwip for sockets. If a target-specific
implementation cannot be found, the build system will fall back to posix. All
files with a .c extension under the selected directory will be compiled. Code
that can be shared among targets can be put in a file named after the feature
with the .c extension. Of course if there is no target-specific code, or if
there are only minimal differences there is not need to create a feature
directory.

    ddsrt
     \- src
         |- atomics.c
         |- sockets
         |   |- posix
         |   |   |- gethostname.c
         |   |   \- sockets.c
         |   \- windows
         |       |- gethostname.c
         |       \- sockets.c
         \- sockets.c

### Development guidelines
* Be pragmatic. Use ifdefs (only) where it makes sense. Do not ifdef if target
  implementations are completely different. Add a seperate implementation. If
  there are only minor differences, as is typically the case between unices,
  use an ifdef.
* Header and source files are not prefixed. Instead they reside in a directory
  named after the module that serves as a namespace. e.g. the threads feature
  interface is defined in `cyclonedds/ddsrt/threads.h`.
* Macros that influence which implementation is used, must be prefixed by
  *DDSRT_USE_* followed by the feature name. e.g. *DDSRT_USE_LWIP* to indicate
  the lwIP TCP/IP stack must be used. Macros that are defined at compile time
  to indicate whether or not a certain feature is available, must be prefixed
  by *DDSR_HAVE_* followed by the feature name. e.g. *DDSRT_HAVE_IPV6* to
  indicate the target supports IPv6 addresses.

### Constructors and destructors
The runtime module (on some targets) requires initialization. For that reason,
`void ddsrt_init(void)` and `void ddsrt_fini(void)` are exported. They are
called automatically when the library is loaded if the target supports it, but
even if the target does not, the application should not need to invoke the
functions as they are called by `dds_init` and `dds_fini` respectively.

Of course, if the runtime module is used by itself, and the target does not
support constructors and/or destructors, the application is required to call
the functions before any of the features from the runtime module are used.

> `ddsrt_init` and `ddsrt_fini` are idempotent. Meaning that, it is safe to
> call `ddsrt_init` more than once. However, initialization is reference
> counted and the number of calls to `ddsrt_init` must match the number of
> calls to `ddsrt_fini`.

#### Threads
Threads require initialization and finalization if not created by the runtime
module. `void ddsrt_thread_init(void)` and `void ddsrt_thread_fini(void)` are
provided for that purpose. Initialization is always automatic, finalization is
automatic if the target supports it. Finalization is primarily used to release
thread-specific memory and call routines registered by
`ddsrt_thread_cleanup_push`.

