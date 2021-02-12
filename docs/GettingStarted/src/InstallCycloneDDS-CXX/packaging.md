### Packaging

To package the build, the config and build steps are different from the build process described above.

When packing, the `-DCMAKE_INSTALL_PREFIX=<install-location>`[^1] option should be added to the configuration, and the `<install-location>` should be the directory to install the IDL compiler to.



During the build step, you must also specify that you want to build the install target.

The build process is as follows:

```
$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX=<idlpp-cxx-install-location> ..
$ cmake --build . --target install
```

After the build, the required files are copied to:

- `<idlpp-cxx-install-location>/lib`
- `<idlpp-cxx-install-location>/share`

The `<install-location>` directory can be used to create the packages.

```
$ cpack
```

This generates the packages corresponding to the target.


---
1: For example, the package can be installed under the build directory, and the command can be `-DCMAKE_INSTALL_PREFIX=$PWD/install ..`