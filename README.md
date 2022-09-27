![GitHub release](https://img.shields.io/github/v/release/eclipse-cyclonedds/cyclonedds?include_prereleases)
[![Build Status](https://dev.azure.com/eclipse-cyclonedds/cyclonedds/_apis/build/status/Pull%20requests?branchName=master)](https://dev.azure.com/eclipse-cyclonedds/cyclonedds/_build/latest?definitionId=4&branchName=master)
[![Coverity Status](https://scan.coverity.com/projects/19078/badge.svg)](https://scan.coverity.com/projects/eclipse-cyclonedds-cyclonedds)
[![Coverage](https://img.shields.io/azure-devops/coverage/eclipse-cyclonedds/cyclonedds/4/master)](https://dev.azure.com/eclipse-cyclonedds/cyclonedds/_build/latest?definitionId=4&branchName=master)
[![License](https://img.shields.io/badge/License-EPL%202.0-blue)](https://choosealicense.com/licenses/epl-2.0/)
[![License](https://img.shields.io/badge/License-EDL%201.0-blue)](https://choosealicense.com/licenses/edl-1.0/)
[![Website](https://img.shields.io/badge/web-cyclonedds.io-blue)](https://cyclonedds.io)
[![Community](https://img.shields.io/badge/discord-join%20community-5865f2)](https://discord.gg/BkRYQPpZVV)


# Eclipse Cyclone DDS

Eclipse Cyclone DDS is a very performant and robust open-source implementation of the [OMG DDS specification](https://www.omg.org/spec/DDS/1.4/About-DDS/).
Cyclone DDS is developed completely in the open as an Eclipse IoT project (see [eclipse-cyclone-dds](https://projects.eclipse.org/projects/iot.cyclonedds)) with a growing list of [adopters](https://iot.eclipse.org/adopters/?#iot.cyclonedds) (if you're one of them, please add your [logo](https://github.com/EclipseFdn/iot.eclipse.org/issues/new?template=adopter_request.md)).
It is a tier-1 middleware for the Robot Operating System [ROS 2](https://docs.ros.org/en/rolling/).

* [What is DDS?](#what-is-dds)
* [Getting Started](#getting-started)
* [Performance](#performance)
* [Configuration](#run-time-configuration)

# What is DDS?

DDS is the best-kept secret in distributed systems, one that has been around for much longer than most publish-subscribe messaging systems and still outclasses so many of them.
DDS is used in a wide variety of systems, including air-traffic control, jet engine testing, railway control, medical systems, naval command-and-control, smart greenhouses and much more.
In short, it is well-established in aerospace and defense but no longer limited to that.
And yet it is easy to use!

Types are usually defined in IDL and preprocessed with the IDL compiler included in Cyclone, but our [Python binding](https://github.com/eclipse-cyclonedds/cyclonedds-python) allows you to define data types on the fly:
```Python
from dataclasses import dataclass
from cyclonedds.domain import DomainParticipant
from cyclonedds.core import Qos, Policy
from cyclonedds.pub import DataWriter
from cyclonedds.sub import DataReader
from cyclonedds.topic import Topic
from cyclonedds.idl import IdlStruct
from cyclonedds.idl.annotations import key
from time import sleep
import numpy as np
try:
    from names import get_full_name
    name = get_full_name()
except:
    import os
    name = f"{os.getpid()}"

# C, C++ require using IDL, Python doesn't
@dataclass
class Chatter(IdlStruct, typename="Chatter"):
    name: str
    key("name")
    message: str
    count: int

rng = np.random.default_rng()
dp = DomainParticipant()
tp = Topic(dp, "Hello", Chatter, qos=Qos(Policy.Reliability.Reliable(0)))
dw = DataWriter(dp, tp)
dr = DataReader(dp, tp)
count = 0
while True:
    sample = Chatter(name=name, message="Hello, World!", count=count)
    count = count + 1
    print("Writing ", sample)
    dw.write(sample)
    for sample in dr.take(10):
        print("Read ", sample)
    sleep(rng.exponential())
```

Today DDS is also popular in robotics and autonomous vehicles because those really depend on high-throuhgput, low-latency control systems without introducing a single point of failure by having a message broker in the middle.
Indeed, it is by far the most used and the default middleware choice in ROS 2.
It is used to transfer commands, sensor data and even video and point clouds between components.

The OMG DDS specifications cover everything one needs to build systems using publish-subscribe messaging.
They define a structural type system that allows automatic endianness conversion and type checking between readers and writers.
This type system also supports type evolution.
The interoperable networking protocol and standard C++ API make it easy to build systems that integrate multiple DDS implementations.
Zero-configuration discovery is also included in the standard and supported by all implementations.

DDS actually brings more: publish-subscribe messaging is a nice abstraction over "ordinary" networking, but plain publish-subscribe doesn't affect how one *thinks* about systems.
A very powerful architecture that truly changes the perspective on distributed systems is that of the "shared data space", in itself an old idea, and really just a distributed database.
Most shared data space designs have failed miserably in real-time control systems because they provided strong consistency guarantees and sacrificed too much performance and flexibility.
The *eventually consistent* shared data space of DDS has been very successful in helping with building systems that need to satisfy many "ilities": dependability, maintainability, extensibility, upgradeability, ...
Truth be told, that's why it was invented, and publish-subscribe messaging was simply an implementation technique.

Cyclone DDS aims at full coverage of the specs and today already covers most of this.
With references to the individual OMG specifications, the following is available:

- [DCPS](https://www.omg.org/spec/DDS/1.4/PDF) the base specification
  - zero configuration discovery (if multicast works)
  - publish/subscribe messaging
  - configurable storage of data in subscribers
  - many QoS settings - liveliness monitoring, deadlines, historical data, ...
  - coverage includes the Minimum, Ownership and (partially) Content profiles
- [DDS Security](https://www.omg.org/spec/DDS-SECURITY/1.1/PDF) - providing authentication, access control and encryption
- [DDS C++ API](https://www.omg.org/spec/DDS-PSM-Cxx/1.0/PDF)
- [DDS XTypes](https://www.omg.org/spec/DDS-XTypes/1.3/PDF) - the structural type system (some [caveats](docs/dev/xtypes_relnotes.md) here)
- [DDSI-RTPS](https://www.omg.org/spec/DDSI-RTPS/2.5/PDF) - the interoperable network protocol

The network stack in Cyclone DDS has been around for over a decade in one form or another and has proven itself in many systems, including large, high-availability ones and systems where interoperation with other implementations was needed.

This repository provides the core of Cyclone DDS including its C API, the [OMG C++](https://github.com/eclipse-cyclonedds/cyclonedds-cxx) and the [Python](https://github.com/eclipse-cyclonedds/cyclonedds-python) language bindings are in sibling repositories.

Consult the [roadmap](ROADMAP.md) for a high-level overview of upcoming features.

# Getting Started

## Building Eclipse Cyclone DDS

In order to build Cyclone DDS you need a Linux, Mac or Windows 10 machine (or, with some caveats, a *BSD, QNX, OpenIndiana or a Solaris 2.6 one) with the following installed on your host:

  * C compiler (most commonly GCC on Linux, Visual Studio on Windows, Xcode on macOS);
  * Optionally GIT version control system;
  * [CMake](https://cmake.org/download/), version 3.16 or later;
  * Optionally [OpenSSL](https://www.openssl.org/), preferably version 1.1;
  * Optionally [Eclipse Iceoryx](https://iceoryx.io) version 2.0 for shared memory and zero-copy support;
  * Optionally [Bison](https://www.gnu.org/software/bison/) parser generator. A cached source is checked into the repository.

If you want to play around with the parser you will need to install the bison parser generator. On Ubuntu `apt install bison` should do the trick for getting it installed.
On Windows, installing chocolatey and `choco install winflexbison3` should get you a long way.  On macOS, `brew install bison` is easiest.

To obtain Eclipse Cyclone DDS, do

    $ git clone https://github.com/eclipse-cyclonedds/cyclonedds.git
    $ cd cyclonedds
    $ mkdir build

Depending on whether you want to develop applications using Cyclone DDS or contribute to it you can follow different procedures:

### Build configuration

There are some configuration options specified using CMake defines in addition to the standard options like `CMAKE_BUILD_TYPE`:

* `-DBUILD_EXAMPLES=ON`: to build the included examples
* `-DBUILD_TESTING=ON`: to build the test suite (this requires [CUnit](http://cunit.sourceforge.net/), see [Contributing to Eclipse Cyclone DDS](#contributing-to-eclipse-cyclone-dds) below for more information)
* `-DBUILD_IDLC=NO`: to disable building the IDL compiler (affects building examples, tests and `ddsperf`)
* `-DBUILD_DDSPERF=NO`: to disable building the [`ddsperf`](https://github.com/eclipse-cyclonedds/cyclonedds/tree/master/src/tools/ddsperf) tool for performance measurement
* `-DENABLE_SSL=NO`: to not look for OpenSSL, remove TLS/TCP support and avoid building the plugins that implement authentication and encryption (default is `AUTO` to enable them if OpenSSL is found)
* `-DENABLE_SHM=NO`: to not look for Iceoryx and disabled shared memory support (default is `AUTO` to enable it if Iceoryx is found)
* `-DENABLE_SECURITY=NO`: to not build the security interfaces and hooks in the core code, nor the plugins (one can enable security without OpenSSL present, you'll just have to find plugins elsewhere in that case)
* `-DENABLE_LIFESPAN=NO`: to exclude support for finite lifespans QoS
* `-DENABLE_DEADLINE_MISSED=NO`: to exclude support for finite deadline QoS settings
* `-DENABLE_TYPE_DISCOVERY=NO`: to exclude support for type discovery and checking type compatibility (effectively most of XTypes), requires also disabling topic discovery using `-DENABLE_TOPIC_DISCOVERY=NO`
* `-DENABLE_TOPIC_DISCOVERY=NO`: to exclude support for topic discovery
* `-DENABLE_SOURCE_SPECIFIC_MULTICAST=NO`: to disable support for source-specific multicast (disabling this and `-DENABLE_IPV6=NO` may be needed for QNX builds)
* `-DENABLE_IPV6=NO`: to disable ipv6 support (disabling this and `-DENABLE_SOURCE_SPECIFIC_MULTICAST=NO` may be needed for QNX builds)
* `-DBUILD_IDLC_XTESTS=NO`: Include a set of tests for the IDL compiler that use the C back-end to compile an idl file at (test) runtime, and use the C compiler to build a test application for the generated types, that is executed to do the actual testing (not supported on Windows)

### For application developers

To build and install the required libraries needed to develop your own applications using Cyclone
DDS requires a few simple steps.
There are some small differences between Linux and macOS on the one
hand, and Windows on the other.
For Linux or macOS:

    $ cd build
    $ cmake -DCMAKE_INSTALL_PREFIX=<install-location> ..
    $ cmake --build .

and for Windows:

    $ cd build
    $ cmake -G "<generator-name>" -DCMAKE_INSTALL_PREFIX=<install-location> ..
    $ cmake --build .

where you should replace `<install-location>` by the directory under which you would like to install Cyclone DDS and `<generator-name>` by one of the ways CMake [generators](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html) offer for generating build files.
For example, "Visual Studio 15 2017 Win64" would target a 64-bit build using Visual Studio 2017.

To install it after a successful build, do:

    $ cmake --build . --target install

which will copy everything to:

  * `<install-location>/lib`
  * `<install-location>/bin`
  * `<install-location>/include/ddsc`
  * `<install-location>/share/CycloneDDS`

Depending on the installation location you may need administrator privileges.

At this point you are ready to use Eclipse Cyclone DDS in your own projects.

Note that the default build type is a release build with debug information included (RelWithDebInfo), which is generally the most convenient type of build to use from applications because of a good mix between performance and still being able to debug things.  If you'd rather have a Debug or pure Release build, set `CMAKE_BUILD_TYPE` accordingly.

### Contributing to Eclipse Cyclone DDS

We very much welcome all contributions to the project, whether that is questions, examples, bug
fixes, enhancements or improvements to the documentation, or anything else really.
When considering contributing code, it might be good to know that build configurations for Azure pipelines are present in the repository and that there is a test suite using CTest and CUnit that can be built locally if desired.
To build it, set the cmake variable `BUILD_TESTING` to on when configuring, e.g.:

    $ cd build
    $ cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
    $ cmake --build .
    $ ctest

Such a build requires the presence of [CUnit](http://cunit.sourceforge.net/).
You can install this yourself, or you can choose to instead rely on the [Conan](https://conan.io) packaging system that the CI build infrastructure also uses.
In that case, install Conan and do:

    $ conan install .. --build missing

in the build directory prior to running cmake.

For Windows, depending on the generator, you might also need to add switches to select the architecture and build type, e.g., `conan install -s arch=x86_64 -s build_type=Debug ..`
This will automatically download and/or build CUnit (and, at the moment, OpenSSL).

## Documentation

The [documentation](https://cyclonedds.io/docs) is still rather limited and some parts of it are still only available in the form of text files in the `docs` directory.
This README is usually out-of-date and the state of the documentation is slowly improving, so it definitely worth hopping over to have a look.

## Building and Running the Roundtrip Example

We will show you how to build and run an example program that measures latency.
The examples are built automatically when you build Cyclone DDS, so you don't need to follow these steps to be able to run the program, it is merely to illustrate the process.

    $ mkdir roundtrip
    $ cd roundtrip
    $ cmake <install-location>/share/CycloneDDS/examples/roundtrip
    $ cmake --build .

On one terminal start the application that will be responding to pings:

    $ ./RoundtripPong

On another terminal, start the application that will be sending the pings:

    $ ./RoundtripPing 0 0 0
    # payloadSize: 0 | numSamples: 0 | timeOut: 0
    # Waiting for startup jitter to stabilise
    # Warm up complete.
    # Latency measurements (in us)
    #             Latency [us]                                   Write-access time [us]       Read-access time [us]
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

The numbers above were measured on Mac running a 4.2 GHz Intel Core i7 on December 12th 2018.
From these numbers you can see how the roundtrip is very stable and the minimal latency is now down to 17 micro-seconds (used to be 25 micro-seconds) on this HW.

# Performance

Reliable message throughput is over 1MS/s for very small samples and is roughly 90% of GbE with 100
byte samples, and latency is about 30us when measured using [ddsperf](src/tools/ddsperf) between two Intel(R) Xeon(R) CPU E3-1270 V2 @ 3.50GHz (that's 2012 hardware ...) running Ubuntu 16.04, with the executables built on Ubuntu 18.04 using gcc 7.4.0 for a default (i.e., "RelWithDebInfo") build.

<img src="https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/assets/performance/20190730/throughput-async-listener-rate.png" alt="Throughput" height="400"><img src="https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/assets/performance/20190730/latency-sync-listener.png" alt="Throughput" height="400">

This is with the subscriber in listener mode, using asynchronous delivery for the throughput
test.
The configuration is a marginally tweaked out-of-the-box configuration: an increased maximum
message size and fragment size, and an increased high-water mark for the reliability window on the
writer side.
For details, see the [scripts](examples/perfscript) directory,
the
[environment details](https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/assets/performance/20190730/config.txt) and the [throughput](https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/assets/performance/20190730/sub.log) and [latency](https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/assets/performance/20190730/ping.log) data underlying the graphs.  These also include CPU usage ([throughput](https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/assets/performance/20190730/throughput-async-listener-cpu.png) and [latency](https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/assets/performance/20190730/latency-sync-listener-bwcpu.png)) and [memory usage](https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/assets/performance/20190730/throughput-async-listener-memory.png).

# Run-time configuration

The out-of-the-box configuration should usually be fine, but there are a great many options that can be tweaked by creating an XML file with the desired settings and defining the `CYCLONEDDS_URI` to point to it.
E.g. (on Linux):

    $ cat cyclonedds.xml
    <?xml version="1.0" encoding="UTF-8" ?>
    <CycloneDDS xmlns="https://cdds.io/config" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://cdds.io/config https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/master/etc/cyclonedds.xsd">
        <Domain Id="any">
            <General>
                <Interfaces>
                    <NetworkInterface autodetermine="true" priority="default" multicast="default" />
                </Interfaces>
                <AllowMulticast>default</AllowMulticast>
                <MaxMessageSize>65500B</MaxMessageSize>
            </General>
            <Discovery>
                <EnableTopicDiscoveryEndpoints>true</EnableTopicDiscoveryEndpoints>
            </Discovery>
            <Internal>
                <Watermarks>
                    <WhcHigh>500kB</WhcHigh>
                </Watermarks>
            </Internal>
            <Tracing>
                <Verbosity>config</Verbosity>
                <OutputFile>cdds.log.${CYCLONEDDS_PID}</OutputFile>
            </Tracing>
        </Domain>
    </CycloneDDS>
    $ export CYCLONEDDS_URI=file://$PWD/cyclonedds.xml

(on Windows, one would have to use `set CYCLONEDDS_URI=file://...` instead.)

This example shows a few things:

* `Interfaces` can be used to override the interfaces selected by default.
  Members are
  * `NetworkInterface[@autodetermine]` tells Cyclone DDS to autoselect the interface it deems best.
  * `NetworkInterface[@name]` specifies the name of an interface to select (not shown above, alternative for autodetermine).
  * `NetworkInterface[@address]` specifies the ipv4/ipv6 address of an interface to select (not shown above, alternative for autodetermine).
  * `NetworkInterface[@multicast]` specifies whether multicast should be used on this interface.
    The default value 'default' means Cyclone DDS will check the OS reported flags of the interface and enable multicast if it is supported.
    Use 'true' to ignore what the OS reports and enable it anyway and 'false' to always disable multicast on this interface.
  * `NetworkInterface[@priority]` specifies the priority of an interface.
    The default value (`default`) means priority `0` for normal interfaces and `2` for loopback interfaces.
* `AllowMulticast` configures the circumstances under which multicast will be used.
  If the selected interface doesn't support it, it obviously won't be used (`false`); but if it does support it, the type of the network adapter determines the default value.
  For a wired network, it will use multicast for initial discovery as well as for data when there are multiple peers that the data needs to go to (`true`).
  On a WiFi network it will use it only for initial discovery (`spdp`), because multicast on WiFi is very unreliable.
* `EnableTopicDiscoveryEndpoints` turns on topic discovery (assuming it is enabled at compile time), it is disabled by default because it isn't used in many system and comes with a significant amount of overhead in discovery traffic.
* `Verbosity` allows control over the tracing, "config" dumps the configuration to the trace output (which defaults to "cyclonedds.log", but here the process id is appended).
  Which interface is used, what multicast settings are used, etc., is all in the trace.
  Setting the verbosity to "finest" gives way more output on the inner workings, and there are various other levels as well.
* `MaxMessageSize` controls the maximum size of the RTPS messages (basically the size of the UDP payload).
  Large values such as these typically improve performance over the (current) default values on a loopback interface.
* `WhcHigh` determines when the sender will wait for acknowledgements from the readers because it has buffered too much unacknowledged data.
  There is some auto-tuning, the (current) default value is a bit small to get really high throughput.

Background information on configuring Cyclone DDS can be found [here](docs/manual/config.rst) and a list of settings is [available](docs/manual/options.md).

# Trademarks

* "Eclipse Cyclone DDS", "Cyclone DDS", "Eclipse Iceoryx" and "Iceoryx" are trademarks of the Eclipse Foundation.
* "DDS" is a trademark of the Object Management Group, Inc.
* "ROS" is a trademark of Open Source Robotics Foundation, Inc.
