## on Windows

Generally speaking, CMake does a good job at guessing which generator to use, but some such as windows require that you supply a specific generator.

For example, only 64-bit libraries are shipped for Windows, by default CMake will generate a 32-bit project, resulting in linker errors. When generating a Visual Studio project and want to generate a b4bit build please append **Win64** to the generator description.

The example below shows how to generate a Visual Studio 2015 project with a 64 bit build.

```
cmake -G "Visual Studio 14 2015 Win64" ..
```

**Note:** CMake generators can also create IDE environments. For instance, the &quot;Visual Studio 14 2015 Win64&quot; will generate a Visual Studio solution file. Other IDE's are also possible, like Eclipse IDE.

CMake will use the CMakeLists.txt in the helloworld directory to create makefiles that fit the native platform.

The real build process of the applications can start:
```
cmake --build .
```

If you want to generate a Release build do:

```
cmake --build . --config "Release"
```

The resulting Publisher and Subscriber applications can be found in `examples\helloworld\build\Release`.

The _Hello World!_ example can now be executed, like described in [Test your installation](InstallCycloneDDS/test-your-installation.html), using the binaries that were just built.