### Packaging

To package the build, the config and build steps are be a bit different from the build process described above.

When packing, the `-DCMAKE_INSTALL_PREFIX=<install-location>`[^1] option should be added to the configuration, and the `<install-location>` be the directory which the IDL compiler will be installed.



During the build step, you have to specify that you want to build the install target as well.

The build process be like:

```
$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX=<idlpp-cxx-install-location> ..
$ cmake --build . --target install
```

After the build, required files will be copied to:

- `<idlpp-cxx-install-location>/lib`
- `<idlpp-cxx-install-location>/share`

Then the `<install-location>` directory can be used to create the packages(s).

```
$ cpack
```

This will generate the packages corresponding to the target.


---
1: For example, the package can be installed under the build directory, and the command can be `-DCMAKE_INSTALL_PREFIX=$PWD/install ..`