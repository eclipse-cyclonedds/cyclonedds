## Building the DDS-CXX Hello World example on Windows

To build the _DDS-CXX Hello World_ example in Windows, it's likely that you have to specify the generator for CMake. For example, to generate a Visual Studio 2017 project, use the following command:

```
$ mkdir build
$ cd build
$ cmake -G “Visual Studio 15 2017 Win64” -DCMAKE_PREFIX_PATH=”<idlpp-cxx-install-location>;<cyclone-install-location>;<cyclonedds-cxx-install-location>” ..
```


CMake will use the CMakeLists.txt in the helloworld directory to create makefiles that fits the native platform.

Subsequently, build the example. It's recommended you provide the config of Visual Studio:

```
$ cmake -build . –config "Release";
```


The _DDS-CXX Hello World_ example application can now be found in the helloworld\build\Releasedirectory, use the method in [Test your CXX installation](InstallCycloneDDS-CXX/test-your-cxx-installation-for-native-installation.html) to check if the application can run successfully.

**Note:** If the _DDS-CXX Hello World_ application fails, please check if the [path](InstallCycloneDDS-CXX/environnement-variable-updates.html) has been set up correctly.