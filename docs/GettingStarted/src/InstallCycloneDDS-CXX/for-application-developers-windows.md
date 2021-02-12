### For Application Developers

To build and install the required libraries needed to develop your applications using the C++ binding for Cyclone DDS:[^1]


```
$ cd build
$ cmake -G "<generator-name>" -DCMAKE_INSTALL_PREFIX=<cyclonedds-cxx-install-location> -DCMAKE_PREFIX_PATH="<idlpp-cxx-install-location>;<cyclonedds-install-location>" -DBUILD_EXAMPLES=On ..
$ cmake --build .
```


**Note:** Replace `<generator-name>` with one of the methods CMake generators offer for generating build files. For example, for &quot;`Visual Studio 16 2019`&quot; target a 64-bit build using Visual Studio 2019. And the command should be:

```
$ cmake -G "Visual Studio 16 2019" -DCMAKE_INSTALL_PREFIX=<cyclonedds-cxx-install-location> -DCMAKE_PREFIX_PATH="<idlpp-cxx-install-location>;<cyclonedds-install-location>" -DBUILD_EXAMPLES=On ..
```

To instal after a successful build:

Depending on the installation location you may need administrator privileges.

```
$ cmake --build . --target install
```


This copies everything to:

- `<cyclonedds-cxx-install-location>/lib`
- `<cyclonedds-cxx-install-location>/bin`
- `<cyclonedds-cxx-install-location>/include/ddsc`
- `<cyclonedds-cxx-install-location>/share/CycloneDDS-CXX`


At this point, you are ready to use Eclipse Cyclone DDS-CXX in your projects.

**Note:** The default build type is a release build with debug information included (`RelWithDebInfo`). This is a convenient type of build to use for applications as it provides a good mix between performance and the ability to debug things. If you prefer have a Debug or pure Release build, set `CMAKE_BUILD_TYPE` accordingly.


---
1: For example, the `<idlpp-cxx-install-location>` and `<cyclonedds-install-location>` can be `C:\idlpp-cxx\build\install` and `C:\cyclonedds\build\install`
If the package still can not be found after specify the path, try removing everything from the build directory and build it again.