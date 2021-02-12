## Building the DDS-CXX Hello World example on Windows

To build the _DDS-CXX Hello World_ example in Windows, it's likely that you have to specify the generator for CMake. For example, to generate a Visual Studio 2017 project, use the following command:

```
$ mkdir build
$ cd build
$ cmake -G “Visual Studio 15 2017 Win64” -DCMAKE_PREFIX_PATH=”<idlpp-cxx-install-location>;<cyclone-install-location>;<cyclonedds-cxx-install-location>” ..
```


CMake uses the CMakeLists.txt in the helloworld directory to create makefiles that fit the native platform.

Subsequently, build the example. We recommend you provide the config of Visual Studio:

```
$ cmake -build . –config "Release"
```


The _DDS-CXX Hello World_ example application can now be found in the `helloworld\build\Release` directory, use the method in [Test your CXX installation](InstallCycloneDDS-CXX/test-your-cxx-installation-for-native-installation.html) to check if the application runs successfully.

**Note:** If the _DDS-CXX Hello World_ application fails, please check the [path](InstallCycloneDDS-CXX/environnement-variable-updates.html) is set up correctly.