## HelloWorld IDL

As explained in chapter 3, the benefits of using IDL language to define data is to have a data model that is independent from the programming languages. The **HelloWorld.idl** IDL file used in chapter 3 can therefore be reused. it will be compiled into to be used within a C++ DDS based applications.

The HelloWorld data type is described in a language independent way and stored in the HelloWorldData.idl file (same as in chapter 3).

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


A subset of the OMG Interface Definition Language (IDL) is used as DDS data definition language. In our simple example, the HellowWorld data model is made of one module `HelloWorldData`. A module can be seen as namespace where data with interrelated semantic is represented together in the same logical set.

The struct Msg is the actual data structure that shapes the data used to build the Topics. As already mentioned, a topic is basically an association between a data type and a string name. The topic name is not defined in the IDL file, but it is defined by the application business logic, at runtime.

In our simplistic case, the data type `Msg` contains two fields: `userID` and `message` payload. The `userID` is used as a unique identification of each message instance. This is done using the `#pragma keylist <datatype>` macro.

The Cyclone DDS-CXX IDL compiler translates the module name into namespaces and structure name into classes.

It also generates code for public accessor functions for all fields mentioned in the IDL struct, separate public constructors, and a destructor: A default (empty) constructor that recursively invokes the constructors of all fields, a copy-constructor that performs a deep-copy from the existing class, a move-constructor that moves all arguments to its members, the destructor recursively releases all fields. It also generates code for assignment operators that recursively construct all fields based on the parameter class (copy and move versions).

```
namespace HelloWorldData
{
    class Msg OSPL_DDS_FINAL
    {
    public:
        int32_t userID_;
        std::string message_;

    public:
        Msg() :
                userID_(0) {}

        explicit Msg(
            int32_t userID,
            const std::string& message) : 
                userID_(userID),
                message_(message) {}

        Msg(const Msg &_other) : 
                userID_(_other.userID_),
                message_(_other.message_) {}

#ifdef OSPL_DDS_CXX11
        Msg(Msg &&_other) : 
                userID_(::std::move(_other.userID_)),
                message_(::std::move(_other.message_)) {}
        Msg& operator=(Msg &&_other)
        {
            if (this != &_other) {
                userID_ = ::std::move(_other.userID_);
                message_ = ::std::move(_other.message_);
            }
            return *this;
        }
#endif
        Msg& operator=(const Msg &_other)
        {
            if (this != &_other) {
                userID_ = _other.userID_;
                message_ = _other.message_;
            }
            return *this;
        }

        bool operator==(const Msg& _other) const
        {
            return userID_ == _other.userID_ &&
                message_ == _other.message_;
        }

        bool operator!=(const Msg& _other) const
        {
            return !(*this == _other);
        }

        int32_t userID() const { return this->userID_; }
        int32_t& userID() { return this->userID_; }
        void userID(int32_t _val_) { this->userID_ = _val_; }
        const std::string& message() const { return this->message_; }
        std::string& message() { return this->message_; }
        void message(const std::string& _val_) { this->message_ = _val_; }
#ifdef OSPL_DDS_CXX11
        void message(std::string&& _val_) { this->message_ = _val_; }
#endif
    };

}
```


Note that when translated into a different programming language, the data will have a different representation that is specific to the target language. For instance, as shown in chapter 3, in C the Helloworld data type will be represented by a C structure. This is the advantage of using a neutral language like IDL to describe the data model. It can be translated into different languages that can be shared between different applications written in different programming languages.