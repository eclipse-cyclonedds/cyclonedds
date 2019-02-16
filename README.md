# Eclipse Cyclone DDS

Eclipse Cyclone DDS is by far the most performant and robust DDS implementation available on the
market. Moreover, Cyclone DDS is developed completely in the open as an Eclipse IoT project
(see [eclipse-cyclone-dds](https://projects.eclipse.org/projects/iot.cyclonedds)).

# Getting Started

## Building Eclipse Cyclone DDS

In order to build Cyclone DDS you need a Linux, Mac or Windows 10 machine with the following
installed on your host:

  * [CMake](https://cmake.org/download/), version 3.7 or later.  (Version 3.6 should work but you
    will have to edit the ``cmake_minimum_required`` version and may have to disable building the
    tests.)
  * [OpenSSL](https://www.openssl.org/), preferably version 1.1 or later.  If you wish, you can
    build without support for OpenSSL by setting DDSC\_ENABLE\_OPENSSL to FALSE on the ``cmake ``
    command line (i.e., ``cmake -DDDSC_ENABLE_OPENSSL=FALSE`` ../src).  In that, there is no need to
    have openssl available.
  * Java JDK, version 8 or later, e.g., [OpenJDK 11](http://jdk.java.net/11/).
  * [Apache Maven](http://maven.apache.org/download.cgi), version 3.5 or later.

The Java-based components are the preprocessor and a configurator tool.  The run-time libraries are
pure C code, so there is no need to have Java available on "target" machines.

To obtain Eclipse Cyclone DDS, do

    $ git clone https://github.com/eclipse-cyclonedds/cyclonedds.git 
    $ cd cyclonedds
    $ mkdir build

Depending on whether you want to develop applications using Cyclone DDS or contribute to it you can
follow different procedures

### For application developers

To build and install the required libraries needed to develop your own applications using Cyclone
DDS requires a few simple steps. There are some small differences between Linux and macOS on the one
hand, and Windows on the other. For Linux or macOS:

    $ cd build
    $ cmake -DCMAKE_INSTALL_PREFIX=<install-location> ../src
    $ cmake --build .

and for Windows:

    $ cd build
    $ cmake -G "<generator-name>" -DCMAKE_INSTALL_PREFIX=<install-location> ../src
    $ cmake --build .

where you should replace ``<install-location>`` by the directory under which you would like to
install Cyclone DDS and ``<generator-name>`` by one of the ways
CMake [generators](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html) offer for
generating build files.  For example, "Visual Studio 15 2017 Win64" would target a 64-bit build
using Visual Studio 2017.

To install it after a successful build, do:
    
    $ cmake --build . --target install

which will copy everything to:

  * ``<install-location>/lib``
  * ``<install-location>/bin``
  * ``<install-location>/include/ddsc``
  * ``<install-location>/share/CycloneDDS``
  * ``<install-location>/etc/CycloneDDS``

Depending on the installation location you may need administrator privileges.

At this point you are ready to use Eclipse Cyclone DDS in your own projects.

Note that the default build type is a release build with debug information included
(RelWithDebInfo), which is generally the most convenient type of build to use from applications
because of a good mix between performance and still being able to debug things.  If you'd rather
have a Debug or pure Release build, set ``CMAKE_BUILD_TYPE`` accordingly.

### Contributing to Eclipse Cyclone DDS

We very much welcome all contributions to the project, whether that is questions, examples, bug
fixes, enhancements or improvements to the documentation, or anything else really.  When considering
contributing code, it might be good to know that build configurations for Travis CI and AppVeyor are
present in the repository and that there is a test suite using CTest and CUnit that can be built
locally if desired.  To build it, set the cmake variable ``BUILD_TESTING`` to on when configuring, e.g.:

    $ cd build
    $ cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ../src
    $ cmake --build .
    $ ctest

Such a build requires the presence of [CUnit](http://cunit.sourceforge.net/).  You can install this
yourself, or you can choose to instead rely on the [Conan](https://conan.io) packaging system that
the CI build infrastructure also uses.  In that case, install Conan and do:

    $ conan install ..

in the build directory prior to running cmake.  For Windows, depending on the generator, you might
also need to add switches to select the architecture and build type, e.g., ``conan install -s
arch=x86_64 -s build_type=Debug ..`` This will automatically download and/or build CUnit (and, at
the moment, OpenSSL).

## Documentation

The documentation is still rather limited, and at the moment only available in the sources (in the
form of restructured text files in ``src/docs`` and Doxygen comments in the header files), or as
a
[PDF](https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/assets/pdf/CycloneDDS-0.1.0.pdf). The
intent is to automate the process of building the documentation and have them available in more
convenient formats and in the usual locations.

## Performance

Median small message throughput measured using the Throughput example between two Intel(R) Xeon(R)
CPU E3-1270 V2 @ 3.50GHz (that's 2012 hardware ...) running Linux 3.8.13-rt14.20.el6rt.x86_64,
connected via a quiet GbE and when using gcc-6.2.0 for a default (i.e., "RelWithDebInfo") build is:

<img src="https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/assets/performance/throughput-polling.png" alt="Throughput" height="400">

This is with the subscriber in polling mode. Listener mode is marginally slower; using a waitset the
message rate for minimal size messages drops to 600k sample/s in synchronous delivery mode and about
750k samples/s in asynchronous delivery mode. The configuration is an out-of-the-box configuration,
tweaked only to increase the high-water mark for the reliability window on the writer side. For
details, see the scripts in the ``performance`` directory and
the
[data](https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/assets/performance/throughput.txt).

There is some data on roundtrip latency below.

## Building and Running the Roundtrip Example

We will show you how to build and run an example program that measures latency.  The examples are
built automatically when you build Cyclone DDS, so you don't need to follow these steps to be able
to run the program, it is merely to illustrate the process.

    $ cd cyclonedds/src/examples/roundtrip
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    
On one terminal start the application that will be responding to pings:

    $ ./RoundtripPong

On another terminal, start the application that will be sending the pings:
    
    $ ./RoundtripPing 0 0 0 
    # payloadSize: 0 | numSamples: 0 | timeOut: 0
    # Waiting for startup jitter to stabilise
    # Warm up complete.
    # Round trip measurements (in us)
    #             Round trip time [us]                           Write-access time [us]       Read-access time [us]
    # Seconds     Count   median      min      99%      max      Count   median      min      Count   median      min
        1     28065       17       16       23       87      28065        8        6      28065        1        0
        2     28115       17       16       23       46      28115        8        6      28115        1        0
        3     28381       17       16       22       46      28381        8        6      28381        1        0
        4     27928       17       16       24      127      27928        8        6      27928        1        0
        5     28427       17       16       20       47      28427        8        6      28427        1        0
        6     27685       17       16       26       51      27685        8        6      27685        1        0
        7     28391       17       16       23       47      28391        8        6      28391        1        0
        8     27938       17       16       24       63      27938        8        6      27938        1        0
        9     28242       17       16       24      132      28242        8        6      28242        1        0
       10     28075       17       16       23       46      28075        8        6      28075        1        0

The numbers above were measured on Mac running a 4.2 GHz Intel Core i7 on December 12th 2018.  From
these numbers you can see how the roundtrip is very stable and the minimal latency is now down to 17
micro-seconds (used to be 25 micro-seconds) on this HW.

# Trademarks

* "Eclipse Cyclone DDS" and "Cyclone DDS" are trademarks of the Eclipse Foundation.

* "DDS" is a trademark of the Object Management Group, Inc.
