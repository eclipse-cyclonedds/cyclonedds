### For Application Developers

To build and install the required libraries needed to develop your own applications using Cyclone DDS requires a few simple steps.[^1]



```
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX=<install-location> ..
$ cmake --build .
```

To install it after a successful build, do:
```
$ cmake --build . --target install
```

This installs step will copy everything to:

- `<install-location>/lib`
- `<install-location>/bin`
- `<install-location>/include/ddsc`
- `<install-location>/share/CycloneDDS`

Depending on the installation location you may need administrator privileges.

At this point, you are ready to use Eclipse Cyclone DDS in your own projects.

Note that the default build type is a release build with debug information included (RelWithDebInfo), which is generally the most convenient type of build to use from applications because of a good mix between performance and still being able to debug things. If you'd rather have a Debug or pure Release build, set `CMAKE_BUILD_TYPE` accordingly.


---

1: For example, if you want to install the libraries in the build/install directory, you can do: cmake `-DCMAKE_INSTALL_PREFIX=$PWD/install ..`