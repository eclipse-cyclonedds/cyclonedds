## Linux and macOS

When considering contributing code, it might be good to know that build configurations for Travis CI and AppVeyor are present in the repository and that there is a test suite using CTest and Google Test that can be built locally if desired. To build it, set the CMake variable `BUILD_TESTING` to on when configuring.

```
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
$ cmake --build .
$ ctest
```


**Note:** If the CMake could not find the Cyclone DDS or IDL package, do:

```
$ cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=<install-location> -DCMAKE_PREFIX_PATH="<idlpp-cxx-install-location>;<cyclonedds-install-location>" -DBUILD_EXAMPLES=On ..
```


And if you like to install the package as well, do:

```
$ cmake --build . --target install
```

Such a build requires the presence of [Google Test](https://github.com/google/googletest). You can install this yourself, or you can choose to instead rely on the [Conan](https://conan.io/) package manager that the CI build infrastructure also uses. In that case, install Conan and do:

```
$ conan install .. --build missing
```

in the build directory prior to running CMake. This will automatically download and/or build Google Test.

The Google Test Conan package is hosted in the Bincrafters Bintray repository. In case this repository was not added to your Conan remotes list yet (and the above-mentioned install command failed because it could not find the Google Test package), you can add the Bintray repository by:

```
$ conan remote add <REMOTE> https://api.bintray.com/conan/bincrafters/public-conan
```


**Note:** Replace `<REMOTE>` with a name that identifies the repository (e.g. bincrafters).