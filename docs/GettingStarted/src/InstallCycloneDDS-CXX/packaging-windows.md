### Packaging

If you want to package the product, then the config and build step will be a bit different than the build process described above.

When packing, the `-DCMAKE_INSTALL_PREFIX=<install-location>`[^1] option should be added to the configuration, and the `<install-location>` be the directory which the IDL compiler is installed.



During the build step, you have to specify that you want to build the install target as well.

The build process should be like:

```
$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX=<idlpp-cxx-install-location> ..
$ cmake -G "<generator-name>" <cmake-config_options> ..
$ cmake --build . --target install
```

After the build, required files be copied to:

- `<idlpp-cxx-install-location>/lib`
- `<idlpp-cxx-install-location>/share`


---
1: For example, the package can be installed under the build directory in a folder named “install”, and the command can be `-DCMAKE_INSTALL_PREFIX=install ..`