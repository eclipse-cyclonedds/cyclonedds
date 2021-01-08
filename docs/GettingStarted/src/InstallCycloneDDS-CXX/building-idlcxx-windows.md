### Building

The CXX IDL compiler is available on GitHub, to obtain the CXX IDL, do:

```
$ git clone https://github.com/ADLINK-IST/idlpp-cxx.git
```

And the build process is:

```
$ cd idlpp-cxx
$ mkdir build
$ cd build
$ cmake -G "<generator-name>" <cmake-config_options> ..
$ cmake --build .
```

**Note:** The `<generator-name>` can be used to specify the CMake generator used to generate the build files. For example, `"Visual studio 15 2017 Win64"` target a 64-bit build using Visual Studio 2017.

**Note:** The `<cmake-config_options>` can be ignored or replaced. A few of the most common options are: `-DCMAKE_INSTALL_PREFIX=<install-location> -DCMAKE_BUILD_TYPE=Debug`


With both `"<generator-name>" <cmake-config_options>` specified, the command look like:

```
$ cmake -G “Visual Studio 15 2017 Win64” -DCMAKE_BUILD_TYPE=Debug ..
```