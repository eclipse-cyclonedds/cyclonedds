## Windows

Set the CMake variable `BUILD_TESTING` to on when configuring, e.g.:

```
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
$ cmake --build .
$ ctest
```


**Note:** To install the Cyclone DDS package:

```
$ cmake -DCMAKE_INSTALL_PREFIX=<install-location> ..
$ cmake --build . --target install
```


This build requires [CUnit](http://cunit.sourceforge.net/). You can install this yourself, or you can choose to instead rely on the [Conan](https://conan.io/) packaging system that the CI build infrastructure also uses. In that case, install Conan in the build directory prior to running CMake:

```
$ conan install .. --build missing
```


The CUnit Conan package is hosted in the [Bincrafters Bintray repository](https://bintray.com/bincrafters/public-conan). If this repository is not in your Conan remotes list (and the above install command failed because it could not find the CUnit package), add the Bintray repository using:

```
$ conan remote add <REMOTE> https://api.bintray.com/conan/bincrafters/public-conan
```


**Note:** Replace `<REMOTE>` with a name that identifies the repository (e.g. bincrafters).

**Note:** depending on the generator, you may also need to add switches to select the architecture and build type, e.g.:

```
$ conan install -s arch=x86_64 -s build_type=Debug ..
```


This automatically downloads and/or builds CUnit (and, at the moment, OpenSSL).