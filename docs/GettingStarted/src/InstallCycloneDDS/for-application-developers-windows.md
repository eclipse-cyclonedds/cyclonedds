### For Application Developers

To build and install the required libraries needed to develop your own applications using Cyclone DDS requires a few simple steps.

```
$ cd build
$ cmake -G &quot;<generator-name>&quot; -DCMAKE_INSTALL_PREFIX=<install-location> ..
$ cmake --build .
```

**Note:** You should replace `<install-location>` with the directory under which you like to install Cyclone DDS and `<generator-name>` by one of the ways CMake [generators](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html) offer for generating build files. For example, &quot;Visual Studio 15 2017 Win64&quot; target a 64-bit build using Visual Studio 2017, and the `<install-location>` can be under the `build\install` directory. With both the `"<generator-name>"` and `"<install-location>"` specified as the example, the command looks like this:
```
$ cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_INSTALL_PREFIX=install ..
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

At this point, you are ready to use Eclipse Cyclone DDS in your projects.

Note that the default build type is a release build with debug information included (RelWithDebInfo), which is generally the most convenient type of build to use from applications because of a good mix between performance and still being able to debug things. If you'd rather have a Debug or pure Release build, set `CMAKE_BUILD_TYPE` accordingly.