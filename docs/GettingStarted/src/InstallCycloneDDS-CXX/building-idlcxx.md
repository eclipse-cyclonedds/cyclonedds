### Building

To build the CXX IDL compiler, go into the folder directory and create a "build" folder to keep all the build files.

```
$ cd idlpp-cxx
$ mkdir build
$ cd build
```

Then use CMake to build the project.

```
$ cmake <cmake-config_options> ..
$ cmake --build .
```

**Note:** The `<cmake-config_options>` can be ignored or replaced. A few of the most common options are: `-DCMAKE_INSTALL_PREFIX=<install-location> -DCMAKE_BUILD_TYPE=Debug`

```
$ cmake -DCMAKE_BUILD_TYPE=Debug ..
```