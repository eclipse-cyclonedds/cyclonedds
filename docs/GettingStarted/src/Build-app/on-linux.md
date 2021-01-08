## on Linux

It is a good practice to build examples or applications out-of-source by creating a `build` directory in the `cyclonedds/build/install/share/CycloneDDS/examples/helloworld` directory.

Configure the build environment:
```
$ mkdir build
$ cd build
$ cmake ../
```

CMake will use the CMakeLists.txt in the helloworld directory to create makefiles that fit the native platform.

The real build process of the applications (**HelloworldPublisher** and **HelloworldSubscriber** in this case) can start:

```
cmake --build .
```


The resulting Publisher and Subscriber applications can be found in `examples/helloworld/build`.

The _Hello World!_ example can now be executed, as described in [Test your installation](InstallCycloneDDS/test-your-installation.html) of the previous chapter.