
## Environnement variable updates

On windows, to be able to run Eclipse Cyclone DDS executables, the required libraries (like `ddsc.dll`) need to be available to the executables. Normally, these libraries are installed in system default locations and it works out-of-the-box. However, if they are not installed in those locations, the library search path has to be changed. This can be achieved by executing the command:[^1]


```
set PATH=<install-location>\bin;%PATH%
```

**Note:** Another way to make the required libraries available to the executables, is to simply copy the required libraries to the executables' directory.


---
1: The `<install-location>` is where you install the Cyclone DDS package in the `cmake -DCMAKE_INSTALL_PREFIX=<install-location> ..` step.