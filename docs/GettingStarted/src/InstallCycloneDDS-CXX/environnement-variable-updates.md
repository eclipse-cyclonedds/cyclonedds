## Environnement variable updates

To be able to run an Eclipse Cyclone DDSCXX application, the required libraries (such as ddsc.dll and ddscxx.dll) need to be available in the executable path. These libraries should be installed in the system default location. However, if they are not installed there, the library search path has to be updated accordingly. On Linux use the command:

```
Set PATH=<cyclonedds-installation-location>\bin;<cyclonedds-cxx-installation-location>\bin
```


**Note:** Another way to make the required libraries available to the executables, is to simply copy the required libraries to the executables' directory.