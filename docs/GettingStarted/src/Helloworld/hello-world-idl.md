## Hello World! IDL

The HelloWorld data type is described in a language independent way and stored in the HelloWorldData.idl file:

```
module HelloWorldData
{
    struct Msg
    {
        long userID; 
        string message;
    };
    #pragma keylist Msg userID
};
```


A subset of the OMG Interface Definition Language (IDL) is used as DDS data definition language. In our simple example, the HellowWorld data model is made of one module `HelloWorldData`. A module can be seen as namespace where data with interrelated semantic is represented together in same logical set.

The `structMsg` is the actual data type that shapes the data used to build Topics. As already mentioned, a topic is basically an association between a data type and a string name. The topic name is not defined in the IDL file, but it is defined by the application business logic, at runtime.

In our simplistic case, the data type Msg contains two fields: `userID` and `message` payload. The `userID` is used as a unique identified of each message instance. This is done using the `#pragma keylist <datatype>` macro.

The Cyclone DDS IDL compiler translates the IDL datatype in a C struct with a name made of the` <ModuleName>_<DataTypeName>` .
```
typedef struct HelloWorldData_Msg
{
    int32_t userID;
    char * message;
} HelloWorldData_Msg;
```

Note that when translated into a different programming language, the data will have a different representation that is specific to target language. For instance, as shown in chapter 7, in C++ the Helloworld data type will be represented by a C++ class. This is the advantage of using a neutral language like IDL to describe the data model.It can be translated into different languages that can be shared between different applications written in different programming languages.