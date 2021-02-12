## Linux and macOS

Set the CMake variable `BUILD_TESTING` to on when configuring.

```
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
$ cmake --build .
$ ctest
```

**Note:** If CMake can not locate the Cyclone DDS or IDL package:

```
$ cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=<install-location> -DCMAKE_PREFIX_PATH="<idlpp-cxx-install-location>;<cyclonedds-install-location>" -DBUILD_EXAMPLES=On ..
```


To install the package:

```
$ cmake --build . --target install
```

This build requires [Google Test](https://github.com/google/googletest). You can install this yourself, or you can choose to instead rely on the [Conan](https://conan.io/) package manager that the CI build infrastructure also uses. In that case, install Conan in the build directory prior to running CMake:

```
$ conan install .. --build missing
```

This automatically downloads and/or builds Google Test.

The Google Test Conan package is hosted in the Bincrafters Bintray repository. If this repository was not added to your Conan remotes list (and the above install command failed because it could not find the Google Test package), you can add the Bintray repository using:

```
$ conan remote add <REMOTE> https://api.bintray.com/conan/bincrafters/public-conan
```


**Note:** Replace `<REMOTE>` with a name that identifies the repository (e.g. bincrafters).