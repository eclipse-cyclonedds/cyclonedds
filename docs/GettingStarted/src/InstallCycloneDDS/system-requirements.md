## System requirements

At the time of writing this document, Eclipse Cyclone DDS supports Linux, macOS, and Windows and is known to work on FreeRTOS and the solaris-like Openindiana OS.

To build the Cyclone DDS implementation, the following software should be installed on your machine.

- C compiler (most commonly GCC on Linux, C compiler (most commonly GCC on Linux, Visual Studio on Windows, Xcode on macOS);
- GIT version control system;
- [CMake](https://cmake.org/download/), version 3.7 or later;
- [OpenSSL](https://www.openssl.org/), preferably version 1.1 or later if you want to use TLS over TCP. You can explicitly disable it by setting ENABLE\_SSL=NO, which is very useful for reducing the footprint or when the FindOpenSSL CMake script gives you trouble;
- Java JDK, version 8 or later, e.g., [OpenJDK](https://jdk.java.net/);
- [Apache Maven](https://maven.apache.org/download.cgi), version 3.5 or later.

On Ubuntu `apt install maven default-jdk` should do the trick for getting Java and Maven installed, and the rest should already be there. On Windows, installing chocolatey and `choco install git cmake openjdk maven` should get you a long way. On macOS, `brew install maven cmake` and downloading and installing the JDK is easiest.

Currently, the only Java-based component in Cyclone is the IDL preprocessor. The run-time libraries are pure code, so there is no need to have Java available on &quot;target&quot; machines. If desired, it is possible to do a build without Java or Maven installed by defining `BUILD_IDLC=NO`, but that effectively only gets you the core library. For the current [ROS 2 RMW layer](https://github.com/ros2/rmw_cyclonedds), that is sufficient.