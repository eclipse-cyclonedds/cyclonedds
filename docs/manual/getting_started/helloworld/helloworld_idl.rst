.. index:: HelloWorld; IDL, IDL; HelloWorld

.. _helloworld_idl:

##############
HelloWorld IDL
##############

The HelloWorld data type is described in a language-independent way and stored 
in the HelloWorldData.idl file:

.. code-block:: omg-idl

    module HelloWorldData
    {
        struct Msg
        {
            @key long userID;
            string message;
        };
    };

The data definition language used for DDS corresponds to a subset of 
the OMG Interface Definition Language (IDL). In our simple example, the HelloWorld data
model is made of one module ``HelloWorldData``. A module can be seen as
a namespace where data with interrelated semantics are represented
together in the same logical set.

The ``structMsg`` is the data type that shapes the data used to
build topics. As already mentioned, a topic is an association between a
data type and a string name. The topic name is not defined in the IDL
file, but the application business logic determines it at runtime.

In our simplistic case, the data type Msg contains two fields:
``userID`` and ``message`` payload. The ``userID`` is used to uniquely identify each message instance. This is done using the
``@key`` annotation.

For further information, refer to :ref:`idl_bm`.

.. tabs::

    .. group-tab:: Core DDS (C)

        The IDL compiler translates the IDL datatype into a C struct with a name made 
        of the\ ``<ModuleName>_<DataTypeName>`` .

        .. code-block:: C

           typedef struct HelloWorldData_Msg
           {
               int32_t userID;
               char * message;
           } HelloWorldData_Msg;

    .. group-tab:: C++

        .. code-block:: C++

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

            #ifdef OSPL_DDS_C++11
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
            #ifdef OSPL_DDS_C++11
                    void message(std::string&& _val_) { this->message_ = _val_; }
            #endif
                };

            }

Generated files with the IDL compiler
=====================================

.. tabs::

    .. group-tab:: Core DDS (C)

        The IDL compiler is a C program that processes .idl files.

        .. code-block:: console

            idlc HelloWorldData.idl

        This results in new ``HelloWorldData.c`` and ``HelloWorldData.h`` files
        that need to be compiled, and their associated object file must be linked
        with the **Hello World!** publisher and subscriber application business
        logic. When using the provided CMake project, this step is done automatically.

        As described earlier, the IDL compiler generates one source and one
        header file. The header file (``HelloWorldData.h``) contains the shared 
        messages' data type. While the source file has no direct use from the 
        application developer's perspective.

        ``HelloWorldData.h``\ \* needs to be included in the application code as
        it contains the actual message type and contains helper macros to
        allocate and free memory space for the ``HelloWorldData_Msg`` type.

        .. code-block:: C

            typedef struct HelloWorldData_Msg
            {
                int32_t userID;
                char * message;
            } HelloWorldData_Msg;

            HelloWorldData_Msg_alloc()
            HelloWorldData_Msg_free(d,o)

        The header file also contains an extra variable that describes the data
        type to the DDS middleware. This variable needs to be used by the
        application when creating the topic.

        .. code-block:: C

            HelloWorldData_Msg_desc

    .. group-tab:: C++

        The IDL compiler is a bison-based parser written in pure C and should be
        fast and portable. It loads dynamic libraries to support different output
        languages, but this is seldom relevant to you as a user. You can use
        ``CMake`` recipes as described above or invoke directly:

        .. code-block:: console

            idlc -l C++ HelloWorldData.idl

        This results in the following new files that need to be compiled and
        their associated object file linked with the Hello *World!* publisher
        and subscriber application business logic:

        -  ``HelloWorldData.hpp``
        -  ``HelloWorldData.cpp``

        When using CMake to build the application, this step is hidden and is
        done automatically. For building with CMake, refer to `building the
        HelloWorld example. <#build-the-dds-C++-hello-world-example>`__

        ``HelloWorldData.hpp`` and ``HelloWorldData.cpp`` files contain the data
        type of messages that are shared.


HelloWorld business logic
=========================

.. tabs::

    .. group-tab:: Core DDS (C)

        As well as the HelloWorldData.h/c generated files, the HelloWorld example
        also contains two application-level source files (subscriber.c and 
        publisher.c), containing the business logic.

    .. group-tab:: C++

        As well as from the ``HelloWorldData`` data type files that the *DDS C++
        Hello World* example used to send messages, the *DDS C++ Hello World!*
        example also contains two application-level source files
        (``subscriber.cpp`` and ``publisher.cpp``), containing the business
        logic.
