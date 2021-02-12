## on Windows

CMake usually knows which generator to use, but with Windows you must supply a specific generator.

For example, only 64-bit libraries are shipped for Windows, by default CMake generates a 32-bit project, resulting in linker errors. When generating a Visual Studio project, if you want to generate a b4-bit build, append **Win64** to the generator description.

The following example shows how to generate a Visual Studio 2015 project with a 64-bit build:

```
cmake -G "Visual Studio 14 2015 Win64" ..
```

**Note:** CMake generators can also create IDE environments. For instance, the &quot;Visual Studio 14 2015 Win64&quot; generates a Visual Studio solution file. Other IDE's are also possible, such as Eclipse IDE.

CMake uses the CMakeLists.txt in the helloworld directory to create makefiles that fit the native platform.

The real build process of the applications can start:
```
cmake --build .
```

To generate a Release build:

```
cmake --build . --config "Release"
```

The resulting Publisher and Subscriber applications can be found in `examples\helloworld\build\Release`.

The _Hello World!_ example can now be executed, as described in [Test your installation](InstallCycloneDDS/test-your-installation.html), using the binaries built.