### Packaging

If you want to package the product, the config and build step is different to the build process.

When packing, the `-DCMAKE_INSTALL_PREFIX=<install-location>`[^1] option should be added to the configuration, and the `<install-location>` is the directory where the IDL compiler is installed.



During the build step, you must also specify to build the install target.

The build process should be similar to the following:

```
$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX=<idlpp-cxx-install-location> ..
$ cmake -G "<generator-name>" <cmake-config_options> ..
$ cmake --build . --target install
```

After the build, the required files are copied to:

- `<idlpp-cxx-install-location>/lib`
- `<idlpp-cxx-install-location>/share`


---
1: For example, the package can be installed under the build directory in a folder named “install”, and the command can be `-DCMAKE_INSTALL_PREFIX=install ..`