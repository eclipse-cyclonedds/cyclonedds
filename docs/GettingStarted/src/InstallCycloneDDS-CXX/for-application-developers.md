### For Application Developers

To build and install the required libraries needed to develop your applications using the C++ binding for Cyclone DDS, do:[^1]


```
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX=<cyclonedds-cxx-install-location> -DCMAKE_PREFIX_PATH="<idlpp-cxx-install-location>;<cyclonedds-install-location>" -DBUILD_EXAMPLES=On ..
$ cmake --build .
```


The `<cyclonedds-cxx-install-location>` is where you like to install the C++ binding for Cyclone DDS.

To install the package after a successful build, do:

```
$ cmake --build . --target install
```

This will copy everything to:

- `<cyclonedds-cxx-install-location>/lib`
- `<cyclonedds-cxx-install-location>/bin`
- `<cyclonedds-cxx-install-location>/include/ddsc`
- `<cyclonedds-cxx-install-location>/share/CycloneDDS-CXX`

Depending on the installation location you may need administrator privileges.

At this point, you are ready to use Eclipse Cyclone DDS-CXX in your projects.

Note that the default build type is a release build with debug information included (RelWithDebInfo), which is generally the most convenient type of build to use from applications because of a good mix between performance and still being able to debug things. If you'd rather have a Debug or pure Release build, set `CMAKE_BUILD_TYPE` accordingly.


---
1: For example, the `<idlpp-cxx-install-location>` and `<cyclonedds-install-location>` can be `${HOME}/idlpp-cxx/build/install and ${HOME}/cyclonedds/build/install`.
If the package still cannot be found after specify the path, try removing everything from the build directory and build it again.