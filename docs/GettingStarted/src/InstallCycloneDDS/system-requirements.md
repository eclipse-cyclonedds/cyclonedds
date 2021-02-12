## System requirements

At the time of writing this document, Eclipse Cyclone DDS supports Linux, macOS, and Windows and is known to work on FreeRTOS and the solaris-like Openindiana OS.

To build the Cyclone DDS implementation, the following software should be installed on your machine:

- C compiler (most commonly GCC on Linux, Visual Studio on Windows, Xcode on macOS)
- GIT version control system
- [CMake](https://cmake.org/download/), version 3.7 or later
- [OpenSSL](https://www.openssl.org/), preferably version 1.1 or later to use TLS over TCP. If you need to reduce the footprint or if you have issues with the FindOpenSSL CMake script, you can explicitly disable this by setting ENABLE\_SSL=NO
- Java JDK, version 8 or later, e.g., [OpenJDK](https://jdk.java.net/)
- [Apache Maven](https://maven.apache.org/download.cgi), version 3.5 or later

On Ubuntu use `apt install maven default-jdk` to install Java and Maven, the rest of the software should already be installed with the Ubuntu system. 

On Windows, to install chocolatey, use `choco install git cmake openjdk maven`, this should install the rest of the required software.

On macOS use `brew install maven cmake` and download and install the JDK.

Currently, the only Java-based component in Cyclone is the IDL preprocessor. The run-time libraries are pure code, so there is no need to have Java available on &quot;target&quot; machines. If desired, it is possible to do a build without Java or Maven installed by defining `BUILD_IDLC=NO`, but that only gives you the core library. For the current [ROS 2 RMW layer](https://github.com/ros2/rmw_cyclonedds), that is sufficient.