## Environnement variable updates

To run an Eclipse Cyclone DDSCXX application, the required libraries (such as ddsc.dll and ddscxx.dll) must be available in the executable path. These libraries should be installed in the system default location. However, if they are not installed there, the library search path must be updated accordingly. On Linux use the command:

```
Set PATH=<cyclonedds-installation-location>\bin;<cyclonedds-cxx-installation-location>\bin
```


**Note:** Alternatively, copy the required libraries to the executables' directory.