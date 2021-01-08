## Building the DDS-CXX Hello World example on Linux and macOS

To build the _DDS-CXX Hello World_ example, the PREFIX\_PATHneeds to be specified, the command be like this:

```
$ mkdir build
$ cd build
$ cmake -DCMAKE_PREFIX_PATH="<idlpp-cxx-install-location>;<cyclone-install-location>;<cyclonedds-cxx-install-location>" ..
$ cmake –-build .
```

The _DDS-CXX Hello World_ example application can now be found in the helloworld/builddirectory, use the method in [Test your CXX installation](InstallCycloneDDS-CXX/test-your-cxx-installation-for-native-installation.html) to check if the application runs successfully.