## Linux and macOS

When considering contributing code, it might be good to know that build configurations for Travis CI and AppVeyor are present in the repository and that there is a test suite using CTest and CUnit that can be built locally if desired. To build it, set the CMake variable   `BUILD_TESTING` to on when configuring, e.g.:

```
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
$ cmake --build .
$ ctest
```


**Note:** After this building the Cyclone DDS package, if you like to install the package as well, do:

```
$ cmake -DCMAKE_INSTALL_PREFIX=<install-location> ..
$ cmake --build . --target install
```


Such a build requires the presence of [CUnit](http://cunit.sourceforge.net/). You can install this yourself, or you can choose to instead rely on the [Conan](https://conan.io/) packaging system that the CI build infrastructure also uses. In that case, install Conan and do:

```
$ conan install .. --build missing
```

in the build directory prior to running CMake.

The CUnit Conan package is hosted in the [Bincrafters Bintray repository](https://bintray.com/bincrafters/public-conan). In case this repository was not added to your Conan remotes list yet (and the above-mentioned install command failed because it could not find the CUnit package), you can add the Bintray repository by:

```
$ conan remote add <REMOTE> https://api.bintray.com/conan/bincrafters/public-conan
```


**Note:** Replace `<REMOTE>` with a name that identifies the repository (e.g. bincrafters).