## The IDL compiler generated files

In the current version of Cyclone DDS the IDL complier is a java application `org.eclipse.cyclonedds.compilers.Idlc` packaged in in `idlc-jar-with-dependencies.jar` that takes an input the IDL file.

```
java -classpath "<install-location>/lib/cmake/CycloneDDS/idlc/idlc-jar-with-dependencies.jar" org.eclipse.cyclonedds.compilers.Idlc HelloWorldData.idl
```

Cyclone DDS provides platform specific utilities to facilitate the data model processing:

**On Windows** : Use the `HelloWorldType` project within the HelloWorld solution.

**On Linux/MacOS** : Use The `make datatype` command.

This results in new **HelloWorldData.c** and **HelloWorldData.h** files that need to be compiled and their associated object file linked with the Hello _World!_ publisher and subscriber application business logic. When using the Cyclone provided CMake project, this step is done automatically.

As described earlier, the IDL compiler generates one source and one header files. The header file (**HelloWorldData.h**) contains the data type of the messages that are shared. While the source file has no direct use form the application developer's perspective.

**HelloWorldData.h** needs to be included by the application code as it contains the actual message type and contains helper macros to allocate and free memory space for the `HelloWorldData_MSG` type.

```
typedef struct HelloWorldData_Msg
{
    int32_t userID; 
    char * message;
} HelloWorldData_Msg;

```

```
HelloWorldData_Msg_alloc()
HelloWorldData_Msg_free(d,o)
```

The header file contains an extern variable that describes the data type to the DDS middleware as well. This variable needs to be used by application when creating the topic.

```
HelloWorldData_Msg_desc
```