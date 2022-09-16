###############
Getting Started
###############

.. image:: /_static/gettingstarted-figures/Cyclone_DDS-Logo.png
   :width: 50%
   :align: center


Installing Eclipse Cyclone DDS
==============================
Eclipse Cyclone DDS is a very performant and robust OMG-compliant Data
Distribution Service (DDS) implementation. Cyclone DDS core is
implemented in C and provides C-APIs to applications. Through its
Cyclonedds-cxx package, the ISO/IEC C++ 2003 language binding is
supported.

This publication provides detailed information about how Install Eclipse Cyclone DDS.
The chapters will cover:

(Add hyperlinks here to these sections) The installation and build process of Cyclone Core including the C-APIs. 
(Add hyperlinks here to these sections) Install the C++ support packages. Short C, C++, and Python tutorials are
detailed to give the reader examples of how the DDS technology is used
with Cyclone to share data.

System prerequisites
~~~~~~~~~~~~~~~~~~~~
Before building the Eclipse Cyclone DDS implementation, make sure you meet all the system prerequisites, 
Failure to meet the prerequisites will cause the build to fail.

Supported platforms
''''''''''''''''''''
At the time of writing this document, Eclipse Cyclone DDS supports
Linux, macOS, and Windows and is known to work on FreeRTOS, QNX, and the
Solaris-like Openindiana OS.

Hardware and Software requirements
''''''''''''''''''''''''''''''''''

Make sure you have the following Hardware and software installed on your machine:

* A C compiler (most commonly GCC or clang on Linux, Visual Studio on Windows, XCode on macOS);
* `Git <https://git-scm.com/>`__ version control system;
* `CMake <https://cmake.org/download/>`__, version 3.10 or later;
* Optionally, `OpenSSL <https://www.openssl.org/>`__, preferably version 1.1 or 
later to use TLS over TCP. If you need to reduce the footprint or have issues with the FindOpenSSL CMake script, you can explicitly
  disable this by setting ENABLE\_SSL=NO
  
Post Installation Requirements
''''''''''''''''''''''''''''''
Add (if any) post installation considerations here


Installing on Linux and macOS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On Linux, install these dependencies with a package manager of your choice:

.. code-block:: bash

    yum install git cmake gcc
    apt-get install git cmake gcc
    aptitude install git cmake gcc
    # or others

On macOS, installing XCode from the App Store should be adequate.

Native Installation for Linux and macOS
'''''''''''''''''''''''''''''''''''''''

To obtain the Eclipse Cyclone DDS:

.. code-block:: bash

    git clone https://github.com/eclipse-cyclonedds/cyclonedds.git
    cd cyclonedds
    mkdir build

Please note: Use the appropriate procedure according to your specific needs. 
For example, a different procedure is required if you develop applications using Cyclone DDS versus 
contributing to it.

For Application Developers
''''''''''''''''''''''''''

You can build and install the required libraries needed to develop your
own applications using Cyclone DDS in a few simple steps.

.. code-block:: bash

    cd build
    cmake -DCMAKE_INSTALL_PREFIX=<install-location> ..
    cmake --build .

To install it after a successful build:

Depending on the installation location, you may need an administrator
privileges.

.. code-block:: bash

    cmake --build . --target install

This install step copies everything to:

-  ``<install-location>/lib``
-  ``<install-location>/bin``
-  ``<install-location>/include/ddsc``
-  ``<install-location>/share/CycloneDDS``

At this point, you are ready to use Eclipse Cyclone DDS in your own
projects.

**Note:** The default build type is a release build with debug
information included (RelWithDebInfo). This is a convenient type of
build to use from applications because of a good mix between performance
and still being able to debug things. If you'd rather have a Debug or
pure Release build, set ``CMAKE_BUILD_TYPE`` accordingly.

If you want to contribute to Cyclone DDS, please refer to `Appendix I
Contributing to Eclipse Cyclone
DDS <#appendix-i-contributing-to-eclipse-cyclone-dds>`__.


Installation on Windows
~~~~~~~~~~~~~~~~~~~~~~~
When installing Eclipse Cyclone DDS on Windows, there are several different types. 
This topic describes installation methods with React Native and product installer for Windows. 

How to Install React Native on Windows
'''''''''''''''''''''''''''''''''''''''

To obtain the Eclipse Cyclone DDS:

.. code-block:: bash

    git clone https://github.com/eclipse-cyclonedds/cyclonedds.git
    cd cyclonedds
    mkdir build

Then, depending on whether you like to develop applications using
Cyclone DDS or contribute to it, you may follow different procedures.

On Windows, to install dependencies using chocolatey, use  ``choco install git cmake``.

For Application Developers
''''''''''''''''''''''''''

You can build and install the required libraries needed to develop your
own applications using Cyclone DDS in a few simple steps.

.. code-block:: bash

    cd build
    cmake -G "<generator-name>" -DCMAKE_INSTALL_PREFIX=<install-location> ..
    cmake --build .

**Note:** Replace ``<install-location>`` with the directory where you
would like to install Cyclone DDS. Replace ``<generator-name>`` with one
of the methods CMake
`generators <https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html>`__
offer for generating build files.

For example, "Visual Studio 15 2017 Win64" targets a 64-bit build using
Visual Studio 2017, and the ``<install-location>`` can be in the
``build\install`` directory. With both the ``<generator-name>`` and
``<install-location>`` specified as the example, the command looks like
this:

.. code-block:: bash

    cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_INSTALL_PREFIX=install ..

To install it after a successful build:

Depending on the installation location, you may need administrator
privileges.

.. code-block:: bash

    cmake --build . --target install

This step will copy everything to:

-  ``<install-location>/lib``
-  ``<install-location>/bin``
-  ``<install-location>/include/ddsc``
-  ``<install-location>/share/CycloneDDS``

At this point, you are ready to use Eclipse Cyclone DDS in your
projects.

**Note:** The default build type is a release build that includes debugging 
information (RelWithDebInfo). 

This is a convenient type of build to use from applications because 
of a good mix between performance and still being able to debug things. 
If you'd rather have a Debug or pure Release build, 
set ``CMAKE_BUILD_TYPE`` accordingly.

If you want to contribute to Cyclone DDS, please refer to `Appendix I
Contributing to Eclipse Cyclone DDS for
Windows. <#appendix-i-contributing-to-eclipse-cyclone-dds>`__

Installation with product installer for Windows
'''''''''''''''''''''''''''''''''''''''''''''''

The Cyclone DDS also provides a product installer, a more simple method 
of installation rather than installing it from GitHub.

To install the Cyclone DDS from the installer:

1. Start the installer, and click 'Next'.

.. image:: /_static/gettingstarted-figures/1.5-1.png
   :align: center


2. Accept the terms, and click'Next'.

.. image:: /_static/gettingstarted-figures/1.5-2.png
   :align: center

3. Choose whether you want to add Cyclone DDS to the system PATH. We
   recommend adding it to the system PATH so your application can
   use the related libraries directly. Select whether to add it for the
   current user or all users and click 'Next.'

.. image:: /_static/gettingstarted-figures/1.5-3.png
   :align: center

4. Set up the directory to install Cyclone DDS. We recommend that you DO
   NOT install it in the ``Program Files`` directory, as it needs
   administrators' permission to write to the folder. Click 'Next'.

.. image:: /_static/gettingstarted-figures/1.5-4.png
   :align: center

5. You are now ready to install the Cyclone DDS. Click 'Install.'

.. image:: /_static/gettingstarted-figures/1.5-5.png
   :align: center

6. Click 'Finish'.

.. image:: /_static/gettingstarted-figures/1.5-6.png
   :align: center


The installation of Cyclone DDS core, C-APIs, and pre-compiler is
complete. 

The following section describes how to test it.

Additional Installation Procedures for Python
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section shows the installation procedure for Eclipse Cyclone DDS Python, wrapping the `Eclipse Cyclone DDS`_ C-API for easy creation of DDS applications.

.. _installing:

Prerequisites
''''''''''''''

CycloneDDS Python requires Python version 3.7 or higher, with 3.11 support provisional. The wheels on PyPi contain a pre-built binary of the CycloneDDS C library and IDL compiler. These have a couple of caveats. The pre-built package:

 * has no support for DDS Security,
 * has no support for shared memory via Iceoryx,
 * comes with generic Cyclone DDS binaries that are not optimized per-platform.

If you need these features or cannot use the binaries for other reasons, you can install the Cyclone DDS Python library from the source. You will need to set the environment variable ``CYCLONEDDS_HOME`` to allow the installer to locate the CycloneDDS C library if it is on a non-standard path. At runtime, we leverage several mechanisms to locate the appropriate library for the platform. If you get an exception about non-locatable libraries or wish to manage multiple CycloneDDS installations, you can use the environment variable ``CYCLONEDDS_HOME`` to override the load location.

How to install pip for Python 3.7
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Install with pip from PyPi.

.. code-block:: shell

    pip install cyclonedds


You can install it from the GitHub link directly:

.. code-block:: shell

    CYCLONEDDS_HOME="/path/to/cyclonedds" pip install git+https://github.com/eclipse-cyclonedds/cyclonedds-python


You will need additional dependencies if you wish to run the test suite or build the documentation. These can be installed using Python installation optional components:

.. code-block:: shell

    git clone https://github.com/eclipse-cyclonedds/cyclonedds-python
    cd cyclonedds-python

    # Testsuite:
    pip install .[dev]
    pytest

    # Documentation
    pip install .[docs]
    cd docs
    sphinx-build ./source ./build
    python -m http.server --directory build

Permission denied errors could occur when you try to access a file from Python without having the necessary permissions. 
To fix this error, it is recommended to use `a virtual environment`_, `poetry`_, `pipenv`_ or `pyenv`_. If you *just* want to get going, you can add ``--user`` to your pip command to install for the current user. See the `Installing Python Modules`_ documentation.


.. _first_app:


.. _Eclipse Cyclone DDS: https://github.com/eclipse-cyclonedds/cyclonedds/
.. _a virtual environment: https://docs.python.org/3/tutorial/venv.html
.. _poetry: https://python-poetry.org/
.. _pipenv: https://pipenv.pypa.io/en/latest/
.. _pyenv: https://github.com/pyenv/pyenv
.. _Installing Python Modules: https://docs.python.org/3/installing/index.html

.. _test_your_installation:

Test your Installation
~~~~~~~~~~~~~~~~~~~~~~~

To test if your installation and configuration are working correctly,
you can use the Cyclone DDS *ddsperf* tool (``ddsperf sanity``), or you
can use the Hello World example. To use the ddsperf tool, refer to
`testing your network
configuration. <#testing-your-network-configuration>`__ The test
using the Hello World example is explained in this section.

Environnement variable updates
''''''''''''''''''''''''''''''

To run Eclipse Cyclone DDS executables on Windows, the required libraries
(like ``ddsc.dll``) must be available to the executables. Typically,
these libraries are installed in system default locations and work
out of the box.
However, the library search path must be changed if they are not installed in those locations. This can be achieved by
executing the  following command:

.. code-block:: PowerShell

    set PATH=<install-location>\bin;%PATH%

**Note:** An alternative to make the required libraries available to the
executables are to copy the necessary libraries for the
executables' directory.

Running the pre-built example
''''''''''''''''''''''''''''''

Eclipse Cyclone DDS includes a simple *Hello World!* application that
can be executed to test your installation. The *Hello World!*
application consists of two executables:

-  ``HelloworldPublisher``
-  ``HelloworldSubscriber``

The *Hello World!* application is located in
``<cyclonedds-directory>\build\bin\Debug`` in Windows, and
``<cyclonedds-directory>/build/bin`` in Linux.

To run the example application, open two console windows and navigate to
the appropriate directory in both console windows. Run
``HelloworldSubscriber`` in one of the console windows using:

(Tabs plugin for Win and Linux)
 **Windows** ``HelloworldSubscriber.exe``

 **Linux** ``./HelloworldSubscriber``

Run ``HelloworldPublisher`` in the other console window using:

(Tabs plugin for Win and Linux)
 **Windows** ``HelloworldPublisher.exe``

 **Linux** ``./HelloworldPublisher``

``HelloworldPublisher`` appears as follows:

.. image:: /_static/gettingstarted-figures/1.6.2-1.png
   :align: center


``HelloworldSubscriber`` appears as follows:

.. image:: /_static/gettingstarted-figures/1.6.2-2.png
   :align: center

**Note:** Cyclone's default behavior is automatically detecting the
first network interface card available on your machine and using it to
exchange the hello world message. Therefore, selecting the correct interface card is essential to ensure that your publisher 
and subscriber applications are on the same network. This is a common issue with multiple network interface cards on machine configurations.

You can override this default behavior by updating the property
``//CycloneDDS/Domain/General/``

``NetworkInterfaceAddress`` in a deployment file (e.g.
``cyclonedds.xml``) that you created to point to it through an OS
environment variable named CYCLONEDDS\_URI. 

More information on this topic can be found on the `GitHub
repository <https://github.com/eclipse-cyclonedds/cyclonedds/blob/master/docs/manual/options.md>`__
and on the configuration section on
https://github.com/eclipse-cyclonedds/cyclonedds.

Want to know more about DDS?
''''''''''''''''''''''''''''

The primary source of information is the OMG website at
`http://www.omg.org <http://www.omg.org/>`__, specifically the `DDS
Getting <http://www.omg.org/gettingstarted/omg_idl.htm>`__\ Startedpage
and the `DDS specification <http://www.omg.org/spec/DDS/>`__.


What's new and planned for Documents and Guides
'''''''''''''''''''''''''''''''''''''''''''''''
 1. Tutorial document

 2. Deployment Guide, for the current version you can refer to https://github.com/eclipse-cyclonedds/cyclonedds/blob/master/docs/manual/options.md

The API reference can be found in the sidebar.

Uninstalling Cyclone DDS
~~~~~~~~~~~~~~~~~~~~~~~~

Uninstallation for Native Installation
''''''''''''''''''''''''''''''''''''''

You can manually remove the install and build directory. In Linux and
macOS in the install or build directory:

.. code-block:: bash

    rm -rf *

Uninstallation for the product installer
''''''''''''''''''''''''''''''''''''''''

Windows
^^^^^^^

On Windows, to uninstall the Cyclone DDS, you can either do it from the
Windows application control panel (Programs and Features in Control
Panel) or by using the product installer; in this case, start-up the
Cyclone DDS product installer, and select 'Remove.'

.. image:: /_static/gettingstarted-figures/1.8.2.1.png
   :align: center

Linux and macOS
^^^^^^^^^^^^^^^

TBD.


Building your first Cyclone DDS Applications
============================================


Building Your First Example
~~~~~~~~~~~~~~~~~~~~~~~~~~~

To test the complete workflow of building a DDS-based application, you
can use a simple *Hello World!*. Although this application does not
reveal all the power of building a data-centric application, it has the
merit of introducing you to the basic steps to create a DDS application.

This chapter will focus on building this example without analyzing the source code.
In the next chapter, we will analyze the source code.

The procedure used to build the *Hello World!* example can also be used
for building your applications.

On Linux, if you have not specified an installation directory, it is
advised to copy the Cyclone DDS examples to your preferred directory.
You can find them in your ``<install-location>`` directory.

Six files are available under the *Hello* *World!* root directory to
support building the example. For this chapter, we mainly describe:

-  ``CMakeLists.txt``
-  ``HelloWorldData.idl``
-  ``publisher.c``
-  ``subscriber.c``

Building the *HelloWorld!* application with CMake
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the previous sections, building the *Hello* *World!* example is done
by default during the Cyclone build process. However, the *Hello*
*World!* example can also be built using the `CMake
tool <http://cmake.org/>`__, although you can build with your
native compilers and preferred toolchains.

`CMake <http://cmake.org/>`__ is an open-source, cross-platform family
of tools designed to build, test, and package software. It is used to
control the software compilation process using simple platform and
compiler-independent configuration files. It also generates native
makefiles, projects, and workspaces of your development environment.
CMake's main strength is built portability. The same CMake input files
build with GNU make, Visual Studio 6,7,8 IDEs, Borland make, nmake, and
XCode, etc...

Another advantage of CMake is building out-of-source. It simply works
out of the box. There are two fundamental reasons to choose this means:

-  Easy cleanup (no cluttering the source tree). Remove the build
   directory to start from scratch.
-  Multiple build targets. It's possible to have up-to-date Debug and
   Release targets without having to recompile the entire tree. It is easy for
   systems that do the cross-platform compilation to have
   up-to-date builds for the host and target platform.

To use CMake, you need to provide a ``CMakeLists.txt``. A sample
CMakeList file can be found within
``<install-location>/share/CycloneDDS/examples/helloworld/``

The content of the ``CMakeLists.txt`` is:

.. code-block:: cmake

    cmake_minimum_required(VERSION 3.5)

    if (NOT TARGET CycloneDDS::ddsc)
        # Find the Cyclone DDS package. If it is not in a default location, try
        # finding it relative to the example where it most likely resides.
        find_package(CycloneDDS REQUIRED PATHS
    "${CMAKE_SOURCE_DIR}/../../")
    endif()

    # This is a convenience function provided by the CycloneDDS package,
    # that will supply a library target related to the given idl file.
    # In short, it takes the idl file, and generates the source files with
    # the proper data types and compiles them into a library.
    idlc_generate(HelloWorldData_lib "HelloWorldData.idl")

    # Both executables have only one related source file.
    add_executable(HelloworldPublisher publisher.c)
    add_executable(HelloworldSubscriber subscriber.c)

    # Both executables need to be linked to the idl data type library and
    # the ddsc API library.
    target_link_libraries(HelloworldPublisher HelloWorldData_lib CycloneDDS::ddsc)
    target_link_libraries(HelloworldSubscriber HelloWorldData_lib CycloneDDS::ddsc)

To build a Cyclone-based application you need to link your business code
with:

-  The ``ddsc`` library that contains the DDS API the application needs.

-  The helper functions and structures that represent your datatypes.
   These helpers are generated by the Cyclone pre-compiler IDL and can
   be accessed through the CMake (``idlc_generate``) call that takes the
   idl file (e.g ``HelloWorld.idl``) as input and packages the datatyped
   helpers in a library (e.g. ``HelloWorldData_lib``).

The ``idlc_generate`` call makes use of how the DDS IDLC-compiler
generates the helpers' functions and structures.

This process is depicted as follows:

.. image:: /_static/gettingstarted-figures/2.2-1.png
   :align: center


The cyclone-based application executable (e.g. ``HelloworldPublisher``)
is built with the CMake ``target_link_libraries()`` call. This call
combines the ``ddsc`` lib, the datatype helper lib, and the application
code lib.

**Note:** CMake attempts to find the ``CycloneDDS`` CMake package in the
default location, two levels above the current source directory. Every
path and dependency is automatically set. CMake uses the default
locations to locate the code CycloneDDS package.

Building the Hello World! Example
'''''''''''''''''''''''''''''''''

Now that the CMakeLists.txt file is completed, the build process can start.

On Linux
^^^^^^^^

It's good practice to build examples or applications out-of-source by
creating a ``build`` directory in the
``cyclonedds/build/install/share/CycloneDDS/examples/helloworld``
directory.

Configure the built environment:

.. code-block:: bash

    mkdir build
    cd build
    cmake ../

CMake uses the CMakeLists.txt in the HelloWorld directory to create
makefiles that fit the native platform.

The real build process of the applications (``HelloworldPublisher`` and
``HelloworldSubscriber`` in this case) can start:

.. code-block:: bash

    cmake --build .

The resulting Publisher and Subscriber applications can be found in
``examples/helloworld/build``.

The *Hello World!* example can now be executed, as described in `Test
your installation <#test-your-installation>`__ in the previous
Chapter

on Windows
^^^^^^^^^^

CMake usually knows which generator to use, but with Windows, you must
supply a specific generator.

For example, only 64-bit libraries are shipped for Windows. By default
CMake generates a 32-bit project, resulting in linker errors. When
generating a Visual Studio project, if you want to create a b4-bit
build, append **Win64** to the generator description.

The following example shows how to generate a Visual Studio 2015 project
with a 64-bit build:

.. code-block:: PowerShell

    cmake -G "Visual Studio 14 2015 Win64" ..

**Note:** CMake generators can also create IDE environments. For
instance, the "Visual Studio 14 2015 Win64" generates a Visual Studio
solution file. Other IDEs are also possible, such as Eclipse IDE.

CMake uses the ``CMakeLists.txt`` in the HelloWorld directory to create
makefiles that fit the native platform.

The actual build process of the applications can start:

.. code-block:: PowerShell

    cmake --build .

To generate a Release build:

.. code-block:: PowerShell

    cmake --build . --config "Release"

The resulting Publisher and Subscriber applications can be found in ``examples\helloworld\build\Release``.

The *Hello World!* example can now be executed, as described in `Test
your installation <#test-your-installation>`__, using the binaries
built.

Hello World! Code anatomy in C
==============================

The previous chapter described the installation process that built
implicitly or explicitly the C *Hello World!* Example. 

This chapter introduces the fundamental concept of DDS. It details the structural code of a
simple system made by an application that publishes keyed messages and
another one that subscribes and reads such data. Each message represents
a data object that is uniquely identified with a unique key and a
payload.

Data-Centric Architecture
~~~~~~~~~~~~~~~~~~~~~~~~~

In a service-centric architecture, to interact, applications need to
know each other's interfaces to share data, share events, and share
commands or replies. These interfaces are modeled as sets of operations
and functions that are managed in centralized repositories. This kind of
architecture creates unnecessary dependencies that create a
tightly coupled system. The centralized interface repositories are
usually seen as a single point of failure.

In a data-centric architecture, your design focuses on the data each
the application produces and decides to share rather than on the Interfaces'
operations and the internal processing that produced them.

A data-centric architecture creates a decoupled system that focuses on
the data and applications states' that need to be shared rather than the
applications' details. In a data-centric system, data and their
associated quality of services are the only contracts that bounds the
applications together. With DDS, the system decoupling is
bi-dimensional, in Space and time.

Space-decoupling derives from the fact that applications do not need to,
either knows the identity of the data produced (or consumers) nor their
logical or their physical location in the network. Under the hood, DDS
runs a zero-configuration, interoperable discovery protocol that
searches matching data readers and data writes that are interested in
the same data topic.

Time-decoupling derives from the fact that, fundamentally, the nature of
communication is asynchronous. Data producers and consumers,
known as DataWriter's and data readers, are not forced to
be active and connected simultaneously to share data. In this
scenario, the DDS middleware can handle and manage data on behalf of the
late joining data readers applications and deliver to it when they
join the system.

Time and space decoupling gives applications the freedom to be plugged
or unplugged in the system at any time, from anywhere, in any order.
This keeps the complexity and administration of a data-centric
architecture relatively low when adding more and more data readers and
DataWriter's applications.

.. _key_steps:

Keys steps to build the Hello World! application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The *Hello World!* example has an effortless 'data layer' with a data
model made of one data type ``Msg`` that represents keyed messages (c,f
next subsection).

To exchange data with Cyclone DDS, applications' business logic needs
to:

1. Declare its participation and involvement in a *DDS domain*. A DDS
   domain is an administrative boundary that defines, scopes, and
   gathers all the DDS applications, data, and infrastructure that must 
   interconnect by sharing the same data space. Each DDS
   domain has a unique identifier. Applications declare their
   participation within a DDS domain by creating a **Domain Participant
   entity**.
2. Create a **Data topic** with the data type described in a data
   model. The data types define the structure of the Topic. The Topic is
   therefore, an association between the topic's name and a datatype.
   QoSs can be optionally added to this association. The concept Topic
   therefore discriminates and categorizes the data in logical classes
   and streams.
3. Create the **Data Readers** and **Writers** entities 
   specific to the topic. Applications may want to change the default
   QoSs. In the Hello world! Example, the ``ReliabilityQoS`` is changed
   from its default value (``Best-effort``) to ``Reliable``.
4. Once the previous DDS computational entities are in place, the
   application logic can start writing or reading the data.

At the application level, readers and writers do not need to be aware of
each other. The reading application, hereby called Subscriber, polls the
data reader periodically, until a publishing application, hereby called
The publisher writes the required data into the shared topic, namely
``HelloWorldData_Msg``.

The data type is described using the OMG IDL.
Language <http://www.omg.org/gettingstarted/omg_idl.htm>`__ located in
``HelloWorldData.idl`` file. Such IDL file is seen as the data model of
our example.

This data model is preprocessed and compiled by Cyclone-DDS IDL-Compiler
to generate a C representation of the data as described in Chapter 2.
These generated source and header files are used by the
``HelloworldSubscriber.c`` and ``HelloworldPublishe.c`` programs to
share the *Hello* *World!* Message instance and sample.

Hello World! IDL
''''''''''''''''

The HelloWorld data type is described in a language-independent way and
stored in the HelloWorldData.idl file:

.. code-block:: omg-idl

    module HelloWorldData
    {
        struct Msg
        {
            @key long userID;
            string message;
        };
    };

The OMG Interface Definition Language (IDL) subset is used as DDS
data definition language. In our simple example, the HelloWorld data
model is made of one module ``HelloWorldData``. A module can be seen as
a namespace where data with interrelated semantics is represented
together in the same logical set.

The ``structMsg`` is the data type that shapes the data used to
build topics. As already mentioned, a topic is an association between a
data type and a string name. The topic name is not defined in the IDL
file, but the application business logic at runtime determines it.

In our simplistic case, the data type Msg contains two fields:
``userID`` and ``message`` payload. The ``userID`` is used to identify each message instance uniquely. This is done using the
``@key`` annotation.

The Cyclone DDS IDL compiler translates the IDL datatype into a C struct
with a name made of the\ ``<ModuleName>_<DataTypeName>`` .

.. code-block:: C

    typedef struct HelloWorldData_Msg
    {
        int32_t userID;
        char * message;
    } HelloWorldData_Msg;

**Note:** When translated into a different programming language, the
data has another representation specific to the target
language. For instance, as shown in chapter 7, in C++, the Helloworld
data type is represented by a C++ class. This advantage of using
a neutral language like IDL to describe the data model. It can be
translated into different languages that can be shared between different
applications written in other programming languages.

Generated files with the IDL compiler
'''''''''''''''''''''''''''''''''''''

In Cyclone DDS, the IDL compiler is a C program that processes .idl files.

.. code-block:: bash

    idlc HelloWorldData.idl


This results in new ``HelloWorldData.c`` and ``HelloWorldData.h`` files
that need to be compiled, and their associated object file must be linked
with the *Hello World!* publisher and subscriber application business
logic. When using the Cyclone provided CMake project, this step is done
automatically.

As described earlier, the IDL compiler generates one source and one
header file. The header file (``HelloWorldData.h``) contains the shared messages' data type. While the source file has no
direct use from the application developer's perspective.

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


The Hello World! Business Logic
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As well as the ``HelloWorldData.h/c`` generated files, the *Hello
World!* example also contains two application-level source files
(``subscriber.c`` and ``publisher.c``), containing the business logic.

*Hello* *World!* Subscriber Source Code
'''''''''''''''''''''''''''''''''''''''

The ``Subscriber.c`` mainly contains the statements to wait for a *Hello
World!* message and reads it when it receives it.

**Note:** The Cyclone DDS ``read`` semantics keep the data sample
in the data reader cache. It is important to remember to use ``take`` where
appropriate to prevent resource exhaustion.

The subscriber application implements the steps defined in:ref:`the Key Steps <key_steps>`.


.. code-block:: C
    :linenos:

    #include "ddsc/dds.h"
    #include "HelloWorldData.h"
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>

    /* An array of one message (aka sample in dds terms) will be used. */
    #define MAX_SAMPLES 1
    int main (int argc, char ** argv) {
      dds_entity_t participant;
      dds_entity_t topic;
      dds_entity_t reader;
      HelloWorldData_Msg *msg;
      void *samples[MAX_SAMPLES];
      dds_sample_info_t infos[MAX_SAMPLES];
      dds_return_t ret;
      dds_qos_t *qos;
      (void)argc;
      (void)argv;

      /* Create a Participant. */
      participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
      DDS_ERR_CHECK (participant, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

      /* Create a Topic. */
      topic = dds_create_topic (participant, &HelloWorldData_Msg_desc,
      "HelloWorldData_Msg", NULL, NULL);
      DDS_ERR_CHECK (topic, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

      /* Create a reliable Reader. */
      qos = dds_create_qos ();
      dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
      reader = dds_create_reader (participant, topic, qos, NULL);
      DDS_ERR_CHECK (reader, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
      dds_delete_qos(qos);

      printf ("\n=== [Subscriber] Waiting for a sample ...\n");

      /* Initialize the sample buffer, by pointing the void pointer within
      * the buffer array to a valid sample memory location. */
      samples[0] = HelloWorldData_Msg alloc ();

      /* Poll until data has been read. */
      while (true)
      {
        /* Do the actual read.
        * The return value contains the number of reading samples. */
        ret = dds_read (reader, samples, infos, MAX_SAMPLES, MAX_SAMPLES);
        DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        /* Check if we read some data and it is valid. */
        if ((ret > 0) && (infos[0].valid_data))
        {
            /* Print Message. */
            msg = (HelloWorldData_Msg*) samples[0];
            printf ("=== [Subscriber] Received : ");
            printf ("Message (%d, %s)\n", msg->userID, msg->message);
            break;
        }
        else
        {
            /* Polling sleep. */
            dds_sleepfor (DDS_MSECS (20));
        }
      }
      /* Free the data location. */
      HelloWorldData_Msg_free (samples[0], DDS_FREE_ALL);

      /* Deleting the participant will delete all its children recursively as well. */
      ret = dds_delete (participant);
      DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

      return EXIT_SUCCESS;
    }

Within the subscriber code, we mainly use the DDS API and the
``HelloWorldData_Msg`` type. The following header files must be included:

* The ``dds.h`` file to give access to the DDS APIs
* The ``HelloWorldData.h`` is specific to the data type defined in the IDL

.. code-block:: C

    #include "ddsc/dds.h"
    #include "HelloWorldData.h"

With Cyclone DDS, at least three DDS entities are needed to build a
minimalistic application, the domain participant, the topic, and the
reader. A DDS Subscriber entity is implicitly created by Cyclone DDS. If
required, this behavior can be overridden.

.. code-block:: C

    dds_entity_t participant;
    dds_entity_t topic;
    dds_entity_t reader;

To handle the data, some buffers are declared and created:

.. code-block:: C

    HelloWorldData_Msg *msg;
    void *samples[MAX_SAMPLES];
    dds_sample_info_t info[MAX_SAMPLES];

As the ``read()`` operation may return more than one data sample (in the
event that several publishing applications are started simultaneously to
write different message instances), an array of samples is therefore
needed.

In Cyclone DDS data and metadata are propagated together. The
``dds_sample_info`` array needs to be declared to handle the metadata.

The DDS participant is always attached to a specific DDS domain. In the
*Hello World!* example, it is part of the ``Default_Domain``, the one
specified in the xml deployment file (see :ref:`test your installation <test_your_installation>` for more details).

.. code-block:: C

    participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);

The next step is to create the topic with a given name. Topics with the
same data type description and with different names are considered
different topics. This means that readers or writers created for a given
topic do not interfere with readers or writers created with another
topic even if they have the same data type. Topics with the same name but
incompatible datatype is considered an error and should be avoided.

.. code-block:: C

    topic = dds_create_topic (participant, &HelloWorldData_Msg_desc, "HelloWorldData_Msg", NULL, NULL);

Once the topic is created, we can create a data reader and attach to it.

.. code-block:: C

    dds_qos_t *qos = dds_create_qos ();
    dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
    reader = dds_create_reader (participant, topic, qos, NULL);
    dds_delete_qos(qos);

The read operation expects an array of pointers to a valid memory
location. This means the samples array needs initialization by pointing
the void pointer within the buffer array to a valid sample memory
location.

In our example, we have an array of one element
(``#define MAX_SAMPLES 1``.) we only need to allocate memory for one
``HelloWorldData_Msg``.

.. code-block:: C

    samples[0] = HelloWorldData_Msg_alloc ();

At this stage, we can attempt to read data by going into a polling loop
that regularly scrutinizes and examines the arrival of a message.

.. code-block:: C

    ret = dds_read (reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);

The ``dds_read`` operation returns the number of samples equal to the
parameter ``MAX_SAMPLE``. If that number exceeds 0 that means data
arrived in the reader's cache.

The Sample\_info (``info``) structure tells us whether the data we are
reading is *Valid* or *Invalid*. Valid data means that it contains the
payload provided by the publishing application. Invalid data means, that
we are rather reading the DDS state of data Instance. The state of a
data instance can be for instance *DISPOSED* by the writer or it is
*NOT\_ALIVE* anymore, which could happen if the publisher application
terminates while the subscriber is still active. In this case, the
sample is not considered as Valid, and its sample ``info[].Valid_data``
field is be ``False``.


.. code-block:: C

    if ((ret > 0) && (info[0].valid_data))

If data is read, then we can cast the void pointer to the actual message
data type and display the contents.


.. code-block:: C

    msg = (HelloWorldData_Msg*) samples[0]; 
    printf ("=== [Subscriber] Received : ");
    printf ("Message (%d, %s)\n", msg->userID, msg->message);
    break;

When data is received and the polling loop is stopped, we release the
allocated memory and delete the domain participant.

.. code-block:: C

    HelloWorldData_Msg_free (samples[0], DDS_FREE_ALL); 
    dds_delete (participant);

All the entities that are created under the participant, such as the
data reader and topic, are recursively deleted.

*Hello* *World!* Publisher Source Code
''''''''''''''''''''''''''''''''''''''

The ``Publisher.c`` contains the source that writes a *Hello World!*
Message.

From the DDS perspective, the publisher application code is almost
symmetric to the subscriber one, except that you need to create a data
writer instead of a data reader. To ensure data is written only when
Cyclone DDS discovers at least a matching reader, a synchronization
statement is added to the main thread. Synchronizing the main thread
until a reader is discovered ensures we can start the publisher or
subscriber program in any order.


.. code-block:: C
    :linenos:

    #include "ddsc/dds.h"
    #include "HelloWorldData.h"
    #include <stdio.h>
    #include <stdlib.h>

    int main (int argc, char ** argv)
    {
        dds_entity_t participant; 
        dds_entity_t topic; 
        dds_entity_t writer; 
        dds_return_t ret;
        HelloWorldData_Msg msg; 
        (void)argc;
        (void)argv;

        /* Create a Participant. */
        participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL); 
        DDS_ERR_CHECK (participant, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        /* Create a Topic. */
        topic = dds_create_topic (participant, &HelloWorldData_Msg_desc, "HelloWorldData_Msg", NULL, NULL); 
        DDS_ERR_CHECK (topic, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        /* Create a Writer. */
        writer = dds_create_writer (participant, topic, NULL, NULL);

        printf("=== [Publisher] Waiting for a reader to be discovered ...\n");

        ret = dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS); 
        DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        while(true)
        {
            uint32_t status;
            ret = dds_get_status_changes (writer, &status); 
            DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

            if (status == DDS_PUBLICATION_MATCHED_STATUS) {
                break;
            }
            /* Polling sleep. */
            dds_sleepfor (DDS_MSECS (20));
        }

        /* Create a message to write. */
        msg.userID = 1;
        msg.message = "Hello World";

        printf ("=== [Publisher]    Writing : ");
        printf ("Message (%d, %s)\n", msg.userID, msg.message);

        ret = dds_write (writer, &msg);
        DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        /* Deleting the participant will delete all its children recursively as well. */
        ret = dds_delete (participant);
        DDS_ERR_CHECK (ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        return EXIT_SUCCESS;
    }

We are using the DDS API and the ``HelloWorldData_Msg`` type to send
data, therefore, we need to include the appropriate header files as we
did in the subscriber code.


.. code-block:: C

    #include "ddsc/dds.h"
    #include "HelloWorldData.h"

Like the reader in ``subscriber.c``, we need a participant and a
topic to create a writer. We must also use the same topic name specified in ``subscriber.c``.


.. code-block:: C

    dds_entity_t participant; 
    dds_entity_t topic; 
    dds_entity_t writer;

    participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL); 
    topic = dds_create_topic (participant, &HelloWorldData_Msg_desc,
    "HelloWorldData_Msg", NULL, NULL); 
    writer = dds_create_writer (participant, topic, NULL, NULL);

When Cyclone DDS discovers readers and writers sharing the same data
type and topic name, it connects them without the application's
involvement. A rendezvous pattern is required to write data only when 
a data reader appears. Either can implement such a pattern:

*  Waiting for the publication/subscription matched events, where the
   Publisher waits and blocks the writing thread until the appropriate
   publication matched event is raised, or
*  Regularly polls the publication matching status. This is the
   the preferred option we implement in this example. The following line of
   code instructs Cyclone DDS to listen on the
   DDS\_PUBLICATION\_MATCHED\_STATUS:

.. code-block:: C

    dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS);

At regular intervals we get the status change and a matching
publication. In between, the writing thread sleeps for a time period
equal ``DDS\_MSECS`` (in milliseconds).

.. code-block:: C

    while(true)
    {
        uint32_t status;
        ret = dds_get_status_changes (writer, &status);
        DDS_ERR_CHECK(ret, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

        if (status == DDS_PUBLICATION_MATCHED_STATUS) {
            break;
        }
        /* Polling sleep. */ 
        dds_sleepfor (DDS_MSECS (20));
    }

After this loop, we are sure that a matching reader has been discovered.
Now, we commence the writing of the data instance. First, the data must
be created and initialized


.. code-block:: C

    HelloWorldData_Msg msg;

    msg.userID = 1;
    msg.message = "Hello World";

Then we can send the data instance of the keyed message.

.. code-block:: C

    ret = dds_write (writer, &msg);

When terminating the program, we free the DDS allocated resources by
deleting the root entity of all the others: the domain participant.

.. code-block:: C

    ret = dds_delete (participant);

All the underlying entities such as topic, writer … etc are deleted.


Hello World! Code Anatomy in C++
================================

To test your installation, the *Hello World* example can be used. The
code of this application is detailed in the next chapter.

The *DDS CXX Hello World* example can be found in the
``<cyclonedds-cxx-install-location>/share/CycloneDDS CXX/helloworld``
directory for both Linux and Windows. This chapter describes the example
build process using the CMake.

Building Eclipse Cyclone DDS CXX applications with CMake
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''

The CMake build file for the *DDS CXX Hello World* example is located
under the ``helloworld`` directory (``CMakeLists.txt``).

The content of the ``CMakeLists.txt`` is as follows:

.. code-block:: cmake

    project(helloworld LANGUAGES C CXX)
    cmake_minimum_required(VERSION 3.5)

    if (NOT TARGET CycloneDDS CXX::ddscxx)
      find_package(CycloneDDS CXX REQUIRED)
    endif()

    # Convenience function, provided by the idlc backend for CXX that generates a CMake
    # target for the given IDL file. The function calls idlc to generate
    # source files and compiles them into a library.
    idlcxx_generate(TARGET ddscxxHelloWorldData_lib FILES HelloWorldData.idl WARNINGS no-implicit-extensibility)

    add_executable(ddscxxHelloworldPublisher publisher.cpp)
    add_executable(ddscxxHelloworldSubscriber subscriber.cpp)

    # Link both executables to idl data type library and ddscxx.
    target_link_libraries(ddscxxHelloworldPublisher ddscxxHelloWorldData_lib CycloneDDS CXX::ddscxx)
    target_link_libraries(ddscxxHelloworldSubscriber ddscxxHelloWorldData_lib CycloneDDS CXX::ddscxx)

    set_property(TARGET ddscxxHelloworldPublisher PROPERTY CXX_STANDARD 11)
    set_property(TARGET ddscxxHelloworldSubscriber PROPERTY CXX_STANDARD 11)

To build a Cyclone DDS CXX based application with CMake, you must link
your application business code with:

-  ``Cyclone DDS CXX`` libraries that contain the DDS CXX API your
   application needs.

-  The wrapper classes and structures that represent your datatypes and
   the customized-DataWriter's and readers that can handle these data
   types. These classes are generated by the CMake statement
   ``idlcxx_generate()`` that incepts the IDL file, invokes the
   IDL compiler and packages the datatype wrapper classes in a library
   (e.g. ``ddscxxHelloWorldData_lib``).

This process is depicted as follows:

.. image:: /_static/gettingstarted-figures/6.1.1-1.png
   :align: center

Setting the property for the applications in the CMake
``set_property()`` statement, compiles the application against the
``C++ 11`` standard.

The application executable (``ddscxxHellowordPublisher``) is built with
the CMake ``target_link_libraries()`` statement which links the ddscxx
lib, the datatype wrapper classes lib (e.g ``ddscxxHelloWorldData_lib``)
and the application code lib.

The CMake tries to find the ``CycloneDDS`` and ``CycloneDDSCXX``
CMake packages, the details regarding how to locate those packages are
described in the next section. When the packages are found, every path
and dependencies are automatically set.

Build the DDS CXX Hello World Example
'''''''''''''''''''''''''''''''''''''

To build the example, navigate to the example's directory and create a
build folder.

.. code-block:: bash

    mkdir build
    cd build

Building the DDS CXX Hello World example on Linux and macOS
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To build the *DDS CXX Hello World* example, the ``PREFIX_PATH`` must be
specified, the command is:


.. code-block:: bash

    mkdir build
    cd build
    cmake -DCMAKE_PREFIX_PATH="<cyclone-install-location>;<cyclonedds-cxx-install-location>" ..
    cmake --build .

The *DDS CXX Hello World* example application can now be found in the
``helloworld/build`` directory, use the method in `Test your CXX
installation <#test-your-cxx-installation-for-native-installation>`__
to check if the application runs successfully.

Building the DDS CXX Hello World example on Windows
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To build the *DDS CXX Hello World* example in Windows, it's likely that
you have to specify the generator for CMake. For example, to generate a
Visual Studio 2017 project, use the following command:

.. code-block:: bash

    mkdir build
    cd build
    cmake -G “Visual Studio 15 2017 Win64” -DCMAKE_PREFIX_PATH=”<cyclone-install-location>;<cyclonedds-cxx-install-location>” ..

CMake uses the CMakeLists.txt in the helloworld directory to create
makefiles that fit the native platform.

Subsequently, build the example. We recommend you provide the config of
Visual Studio:

.. code-block:: bash

    cmake --build . --config "Release"

The *DDS CXX Hello World* example application can now be found in the
``helloworld\build\Release`` directory, use the method in `Test your CXX
installation <#test-your-cxx-installation-for-native-installation>`__
to check if the application runs successfully.

**Note:** If the *DDS CXX Hello World* application fails, please check
the `environment variable <#test-your-cxx-installation-for-native-installation>`__ is set up correctly.


Hello World! Code Anatomy in Python
===================================
This section details how to create your first DDS application in Python API.

Your first Python DDS application
''''''''''''''''''''''''''''''''''

Let's enter the world of DDS by making our presence known. We will not worry about configuration or what DDS does under the hood but write a single message. 
To publish anything to DDS we need to define the type of message first. Suppose you are worried about talking to other applications that are not necessarily running Python. In that case, you will use the CycloneDDS IDL compiler, but for now, we will manually define our message type directly in Python using the ``cyclonedds.idl`` tools:

.. code-block:: python3
    :linenos:

    from dataclasses import dataclass
    from cyclonedds.idl import IdlStruct

    @dataclass
    class Message(IdlStruct):
        text: str


    name = input("What is your name? ")
    message = Message(text=f"{name} has started his first DDS Python application!")


With ``cyclonedds.idl`` write typed classes with the standard library module `dataclasses <python:dataclasses>`. 
For this simple application, you put in a piece of text, but this system has the same expressive power as the OMG IDL specification, allowing you to use almost any complex datastructure.

To send your message over a DDS domain, carry out the following steps:

1. Join the DDS network using a DomainParticipant
2. Define which data type and under what name you will publish your message as a Topic
3. Create the DataWriter that publishes that Topic
4. And finally, publish the message.


.. code-block:: python3
    :linenos:

    
    from cyclonedds.topic import Topic
    from cyclonedds.pub import DataWriter

    participant = DomainParticipant()
    topic = Topic(participant, "Announcements", Message)
    writer = DataWriter(participant, topic)

    writer.write(message)

You have now published your first message successfully! However, it is hard to tell if that did anything, since we don't have anything set up that is listening. Let's make a second script that takes messages from DDS and prints them to the terminal:

.. code-block:: python3
    :linenos:

    from dataclasses import dataclass
    from cyclonedds.domain import DomainParticipant
    from cyclonedds.topic import Topic
    from cyclonedds.sub import DataReader
    from cyclonedds.util import duration
    from cyclonedds.idl import IdlStruct

    @dataclass
    class Message(IdlStruct):
        text: str

    participant = DomainParticipant()
    topic = Topic(participant, "Announcements", Message)
    reader = DataReader(participant, topic)

    # If we don't receive a single announcement for five minutes we want the script to exit.
    for msg in reader.take_iter(timeout=duration(minutes=5)):
        print(msg.text)

Now with this script running in a second terminal, you should see the message pop up when you rerun the first script.


Benchmarking Tools for Cyclone
==============================

Introduction
~~~~~~~~~~~~

Cyclone DDS provides a tool that measures primarily data *throughput*
and *latency* of the cyclone-based applications within the network or
within the same board, namely *ddsperf*. This tool also help to do
sanity checks to ensure your configuration is correctly set up and
running. This chapter describes how to use the *ddsperf* tool and how to
read and interpret its outputs and results. Using the Cyclone DDS Python
package you can also run *ddsperf* as a graphical application, by running
*cyclonedds performance*.

As well as *ddsperf*, you can also find dedicated examples in the
product distribution that measures the DDS system throughput and the
latency with their associated codebase. You can start from the provided
code and customize it to fit your scenario and exact data types. Both
*ddsperf*\ tool and the provided examples perform the benchmarking using
sequences of octets with different parameterized sizes.

Testing your network configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once your Cyclone DDS installation is successfully completed, you may
want to test if your network environment is correctly set up. This can
be done either by running the *HelloWorld* example or by using the
*ddsperf* tool. The Helloworld example sends a message in one shot,
whereas the ddsperf tool can send a continuous stream of data at a low
frequency rate for sanity checks and can therefore bypass sporadic
network issues.

If you have installed Cyclone DDS using the product installer, the
*ddsperf* tool is pre-installed within ``<cyclonedds_dir>/bin``. If you
have installed Cyclone DDS through the process,
(from GitHub), you can locate the tool within
``<cyclonedds_dir>/build/bin``.

Complete the sanity checks of your DDS based system using the *ddsperf*
tool as follows:

.. code-block:: bash

    ddsperf sanity

With the sanity option, only one data sample is sent each second (1Hz).

In another terminal, start the *ddsperf* with the **Pong** mode to echo
the data to the first instance of the *ddsperf* started with the
*Sanity* option.

.. code-block:: bash

    ddsperf pong

.. image:: /_static/gettingstarted-figures/4.2-1.png
   :align: center

If the data is not exchanged on the network between the two ddsperf
instances, it is likely that Cyclone DDS has not selected the
appropriate network card on both machines or a firewall in-between is
preventing the communication.

Cyclone DDS automatically selects the most available network interface.
This behavior can be overridden by changing the configuration file. (see
section :ref:`test your installation <test_your_installation>` for more details) .

When running the previous scenario on a local machine, this test ensures
the loop-back option is enabled.

Measuring Latency
~~~~~~~~~~~~~~~~~

To measure latency between two different applications, you need to run
two instances of the *ddsperf* tool and instruct one of them to endorse
the role of a *sender* that sends a given amount of data (a sequence of
octets) at a given rate and the other instance takes the role of
*receiver* that sends back the same amount of data to the sender in a
Ping-Pong scenario. The sending action is triggered by the **Ping**
option. The receiving behavior is triggered by the **Pong** action. The
sender measures the roundtrip time and computes the latency as half of
the roundtrip time.

The Ping-Pong scenario avoids clock desynchronization issues that might
occur between two machines that do not share accurately the same
perception of the time in the network.

.. image:: /_static/gettingstarted-figures/4.3-1.png
   :align: center

To differential the two operational modes, the *ddsperf* tool can
operate either in a **Ping mode** or in a **Pong mode**.

To run this scenario, open 2 terminals (e.g on Linux like OSs) and run
the following commands in either of the terminals. The graphical
*python-based* alternative is also noted.

.. code-block:: bash

    ddsperf ping
    cyclonedds performance ping

Input this command in another terminal:

.. code-block:: bash

    ddsperf pong
    cyclonedds performance pong

This basic scenario performs a simple latency test with all the default
values. You may customize your test scenario by changing the following
options.

* In **Ping mode** you can specify:

  * The **Rate** and frequency at which data is written. This is
    specified through the [R[Hz]] option. The default rate is "as fast as
    possible". In **ping** mode, it always sends a new ping as soon as it
    gets a pong

  * The **Size** of the data that is exchanged. This is specified
    through the [Size S] option. Using the default built-in topic, 12
    bytes (an integer key, an integer sequence number, and an empty
    sequence of bytes). are sent every time. The size is "as small as
    possible" by default, depending on the size of the topic it defaults
    to

  * The **Listening** mode, which can either be ``waitset`` based or
    ``Listener`` Callbacks modes. In the waitset mode the *ddsperf*
    application creates a dedicated thread to wait for the data to return
    back from the receiving instance of *ddsperf* tool (i.e the instance
    started with the Pong mode). In the Listener Callback mode, the
    thread is created by the Cyclone DDS middleware. The Listener mode is
    the default.

* In **Pong mode** you can only specify one option:

  * The **Listening** mode (with two possible values, ``waitset`` or
    ``Listener``)


For instance, if you want to measure local latency between to processes
exchanging 2KB at the frequency of 50Hz, you can run the following
commands in 2 different terminals:

.. code-block:: bash

    ddsperf ping 50Hz 2048 waitset
    cyclonedds performance ping --rate 50Hz --size 2048 --triggering-mode waitset

.. code-block:: bash

    ddsperf pong waitset
    cyclonedds performance pong --triggering-mode waitset

The output of the *ddsperf* tool is as shown below:

1. The output for the **Ping** application indicates mainly:

-  The **size of the data** involved in the test (e.g. 12 bytes)
-  The **minimum latency** (e.g. 78.89 us)
-  The **maximum latency** (e.g. 544,85 us)
-  The **mean latency** (e.g. 118.434 us)
-  As well as the latency at 50%, 90% or 99% of the time.

.. image:: /_static/gettingstarted-figures/4.3-2.png
   :align: center

2. The output for the **Pong** application:

-  **RSS** is the Resident Set Size; it indicates the amount of memory
   used by the process (e.g. 3.5MB used by the process id 2680);
-  **VCSW** is the number of voluntary switches, it indicates the
   times when the process waits for input or an event (e.g. 2097 times);
-  **IVCSW** is the number of involuntary switches, it indicates the
   times when the process is pre-empted or blocked by a mutex (e.g. 6
   times);
-  The percentage of time spent executing user code and the percentage
   of time spent executing kernel code in a specific thread (e.g. spent
   almost 0% of the time executing the user code and 5% executing kernel
   code in thread "ping").

.. image:: /_static/gettingstarted-figures/4.3-3.png
   :align: center

Measuring Throughput
~~~~~~~~~~~~~~~~~~~~

To measure throughput between two different applications, you need to
run at least two instances of the *ddsperf*\ tool and instruct one of
them to endorse the role of a Publisher that sends a given amount of
data (a sequence of octets) at a given rate. The other instances take
the role of Subscriber applications. Please note that when your scenario
involves only one subscriber, the UDP unicast mode is used. If several
subscriber instances are running, the multicast is automatically used.

.. image:: /_static/gettingstarted-figures/4.4-1.png
   :align: center

Two additional modes are therefore supported:

The **Pub** mode and the **Sub** mode.

In the Sub mode the subscriber operates either:

-  Using the **Listener** notification mechanism,
-  The **WaitSet** notification mechanism, or
-  The **Pooling** mode. The pooling mode allows the subscriber to
   cyclically fetch the data from its local cache instead of being
   notified each time a new set of data is added to the subscriber's
   cache as is the case with the other modes.

You can publish data in two ways by publishing each data sample
individually or by sending them in a *Burst* mode.

-  The **Rate** and frequency at which data is written. This is
   specified through the [R[Hz]] option. The default rate is "as fast as
   possible". Which means, in **pub** mode, instead of trying to reach a
   certain rate, it just pushes data as hard as it can.

-  The **Size** of the data that is exchanged. This is specified through
   the [Size S] option. The size is "as small as possible" by default,
   depending on the size of the topic it defaults to.
-  The **Burst Size** , defines the number of data samples that are
   issued together in as a batch. This parameter is defined by the
   [Burst N] option. The default size for burst is 1. It doesn't make
   much difference when going "as fast as possible", and it only applies
   to the **pub** mode.
-  The triggering mode by default is *listener* for the **ping** ,
   **pong** and **sub** mode.

To run a simple throughput test, you can simply run a **pub** mode and a
**sub** mode in 2 different terminals without specifying any other
options or you can customize it as shown below:

Open two terminals, navigate to the directory where *ddsperf* is located
and write the following command:

.. code-block:: bash

    ddsperf pub size 1k
    cyclonedds performance publish --size 1k

And in the other terminal, type in:

.. code-block:: bash

    ddsperf -Qrss:1 sub
    cyclonedds performance -Qrss:1 subscribe

This measures the throughput of data samples with 1Kbytes written as
fast as possible.

The ``-Qrss:1`` option in **sub** mode sets the maximum allowed increase
in RSS as 1MB. When running the test, if the memory occupieds by the
process increases by less than 1MB, the test can successfully run.
Otherwise, an error message is printed out at the end of the test.

As the ``pub`` in this example only has a size of 1k, the sub does not
print out an RSS error message at the end of the test.

The output of the *ddsperf* tool when measuring throughput is as shown
below:

1. The output for the **Pub** application indicates mainly:

-  **RSS** is the Resident Set Size; it indicates the amount of memory
   is used by the process (e.g. 6.3MB used by the process id "4026");
-  **VCSW** is the number of voluntary switches, it indicates the
   times when the process waits for input or an event (e.g. 1054 times);
-  **IVCSW** is the number of involuntary switches, it indicates the
   times when the process is pre-empted or blocked by a mutex (e.g. 24
   times);
-  The percentage of time spent executing user code and the percentage
   of time spent executing kernel code in a specific thread (e.g. spent
   34% of the time executing the user code and 11% executing kernel code
   in thread "pub").

.. image:: /_static/gettingstarted-figures/4.4-2.png
   :align: center

2. The output for the **Sub** application indicates mainly:

-  The **size of the data** involved in this test (e.g. 1024 bytes,
   which is the "size 1k" defined in the pub command)
-  The **total packets received** (e.g. 614598);
-  The **total packets lost** t (e.g. 0);
-  The **packets received in a 1 second reporting period** (e.g.
   212648);
-  The **packets lost in a 1 second report period** (e.g. 0);
-  The **number of samples processed by the Sub application** in 1s
   (e.g. 21260 KS/s, with the unit KS/s is 1000 samples per second).

.. image:: /_static/gettingstarted-figures/4.4-3.png
   :align: center


Measuring Throughput and Latency in a mixed scenario
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In some scenarios, it might be useful to measure the throughput and
latency at the same time.

The *ddsperf* tool allows you to mix these two scenarios. To address
such cases the Ping mode can be combined with the Pub mode.

The [Ping x%] switch combined with the Pub mode allows you to send a
fraction of samples x% as if they were used in the Ping mode.

The different modes of the *ddsperf* tool are summarized in the figure
below.

.. image:: /_static/gettingstarted-figures/4.5-1.png
   :align: center

You can get more information for the *ddsperf* tool by using the [help]
option:

.. code-block:: bash

    ddsperf help
    cyclonedds performance --help

Additional options
~~~~~~~~~~~~~~~~~~

As well as selecting the ``mode``, you can also select the ``options``
to specify how to send and receive the data (such as modifying the
reliable QoS from Reliable to Best-Effort with the ``-u`` option), or
how to evaluate or view the data in the *ddsperf*\ tool.

The ``options`` you can select are listed in the *ddsperf* ``help``
menu, as shown below.

.. image:: /_static/gettingstarted-figures/4.6-1.png
   :align: center

Installing Eclipse Cyclone DDS CXX
==================================

Cyclone DDS CXX is an implementation of the DDS ISO/IEC C++ PSM API,
or simply put, a C++ binding for Eclipse Cyclone DDS. It is made of an
IDL compiler that uses an IDL data model to generate their C++
representation and artifacts, a software layer that maps some DDS APIs
on the Cyclone DDS C APIs and direct access to the cyclone kernel APIs
when managing data to lower overhead.

.. image:: /_static/gettingstarted-figures/5-1.png
   :align: center

System requirements
~~~~~~~~~~~~~~~~~~~

At the time of writing this document, Eclipse Cyclone DDS CXX supports
Linux, macOS, and Windows. Cyclone DDS CXX is known to work on FreeRTOS
and the solaris-like Openindiana OS.

To build the Cyclone DDS C++ binding, the following software should be
installed on your machine.

-  C and C++ compilers (most commonly GCC on Linux, Visual Studio on
   Windows, Xcode on macOS);
-  `Git <https://git-scm.com/>`__ version control system,
   `CMake <https://cmake.org/download/>`__\ (version 3.7 or later);
-  Eclipse Cyclone DDS

The installation of `Eclipse Cyclone
DDS <#installing-eclipse-cyclone-dds>`__ with the C language support
is described in Chapter 1. This chapter describes the CXX IDL compiler.

Native Installation for Linux and macOS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Install Eclipse Cyclone DDS CXX
'''''''''''''''''''''''''''''''

To obtain the C++ binding for Cyclone DDS:

.. code-block:: bash

    git clone https://github.com/eclipse-cyclonedds/cyclonedds-cxx.git

Building
^^^^^^^^

To build Cyclone DDS CXX, browse to the folder directory and create
a "build" folder to retain the build files.

.. code-block:: bash

    cd cyclonedds-cxx
    mkdir build

Depending on whether you want to develop applications using Cyclone DDS
CXX or contribute to it, you may follow different procedures.

For Application Developers
^^^^^^^^^^^^^^^^^^^^^^^^^^

To build and install the required libraries needed to develop your
applications using the C++ binding for Cyclone DDS:

.. code-block:: bash

    cd build
    cmake -DCMAKE_INSTALL_PREFIX=<cyclonedds-cxx-install-location> -DCMAKE_PREFIX_PATH="<cyclonedds-install-location>" -DBUILD_EXAMPLES=ON ..
    cmake --build .

The ``<cyclonedds-cxx-install-location>`` is where the C++ binding for
Cyclone DDS is installed to.

To install the package after a successful build:

Depending on the installation location you may need administrator
privileges.

.. code-block:: bash

    cmake --build . --target install

This copies everything to:

-  ``<cyclonedds-cxx-install-location>/lib``
-  ``<cyclonedds-cxx-install-location>/bin``
-  ``<cyclonedds-cxx-install-location>/include/ddsc``
-  ``<cyclonedds-cxx-install-location>/share/CycloneDDS CXX``

At this point, you are ready to use Eclipse Cyclone DDS CXX in your
projects.

**Note:** The default build type is a release build with debug
information included (``RelWithDebInfo``). This is a convenient type of
build to use for applications as it provides a good mix between
performance and the ability to debug things. If you'd rather have a
Debug or pure Release build, set ``CMAKE_BUILD_TYPE`` accordingly.

If you want to contribute to Cyclone DDS CXX, please refer to `Appendix
II Contributing to Eclipse Cyclone DDS
CXX <#appendix-ii-contributing-to-eclipse-cyclone-dds-cxx>`__.

Installation with product installer for Linux and macOS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When installing the product from a (.deb) package on the default location, run the command below to build the C or C++ examples:

.. code-block:: bash

1. Copy the example from /usr/share/CycloneDDSPro to your preferred location (e.g to your <Instal_DIR>) 
2. Browse to the folder directory and create a "build" directory to retain all the build files: 
        <Instal_DIR>/CycloneDDSPro/CycloneDDS/examples/helloworld
     then call cmake , > cmake ../
3. Build the example using > cmake --build 

You have successfully installed the product from a (.deb) and built your C or C++ example.

Native Installation for Windows
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Install Eclipse Cyclone DDS CXX
'''''''''''''''''''''''''''''''

To obtain the C++ binding for Cyclone DDS:

.. code-block:: bash

    git clone https://github.com/eclipse-cyclonedds/cyclonedds-cxx.git

Building
^^^^^^^^

To build the Cyclone DDS CXX, browse to the folder directory and create
a "build" folder to retain all the build files, run the command below:

.. code-block:: bash

    cd cyclonedds-cxx
    mkdir build

Depending on whether you want to develop applications using Cyclone
DDS CXX or contribute to it, you may follow different procedures.

For Application Developers
^^^^^^^^^^^^^^^^^^^^^^^^^^

To build and install the required libraries needed to develop your
applications using the C++ binding for Cyclone DDS:

.. code-block:: bash

    cd build
    cmake -G "<generator-name>" -DCMAKE_INSTALL_PREFIX=<cyclonedds-cxx-install-location> -DCMAKE_PREFIX_PATH="<cyclonedds-install-location>" -DBUILD_EXAMPLES=ON ..
    cmake --build .

**Note:** Replace ``<generator-name>`` with one of the methods CMake
generators offer for generating build files. For example, for
"``Visual Studio 16 2019``\ " target a 64-bit build using Visual Studio
2019. And the command should be:

.. code-block:: bash

    cmake -G "Visual Studio 16 2019" -DCMAKE_INSTALL_PREFIX=<cyclonedds-cxx-install-location> -DCMAKE_PREFIX_PATH="<cyclonedds-install-location>" -DBUILD_EXAMPLES=ON ..

To install after a successful build:

Depending on the installation location you may need administrator
privileges.

.. code-block:: bash

    cmake --build . --target install

This copies everything to:

-  ``<cyclonedds-cxx-install-location>/lib``
-  ``<cyclonedds-cxx-install-location>/bin``
-  ``<cyclonedds-cxx-install-location>/include/ddsc``
-  ``<cyclonedds-cxx-install-location>/share/CycloneDDS CXX``

At this point, you are ready to use Eclipse Cyclone DDS CXX in your
projects.

**Note:** The default build type is a release build with debug
information included (``RelWithDebInfo``). This is a convenient type of
build to use for applications as it provides a good mix between
performance and the ability to debug things. If you prefer have a Debug
or pure Release build, set ``CMAKE_BUILD_TYPE`` accordingly.

If you want to contribute to Cyclone DDS CXX, refer to `Contributing to
Eclipse Cyclone DDS CXX for Windows in Appendix
II. <#appendix-ii-contributing-to-eclipse-cyclone-dds-cxx>`__

Installation with product installer for Windows
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Cyclone DDS CXX also provides a product installer, this may be easier
than installing it from GitHub.

To install the Cyclone DDS CXX from the installer, use the following
steps.

1. Start up the installer and click 'Next'.

.. image:: /_static/gettingstarted-figures/5.5-1.png
   :align: center

2. Agree to the terms, and click 'Next'.

.. image:: /_static/gettingstarted-figures/5.5-2.png
   :align: center

3. Choose whether you would like to add Cyclone DDS CXX to the system
   PATH. We recommend to add it to the system PATH, so that your
   application can use the related libraies directly. Select whether you
   would like to add it for the current user or for all users and click
   'Next'.

.. image:: /_static/gettingstarted-figures/5.5-3.png
   :align: center

4. Select the directory where you would like to install Cyclone DDS CXX.
   Avoid installing it within the ``Program Files`` directory, as it
   requires administrator privileges. Click 'Next'.

.. image:: /_static/gettingstarted-figures/5.5-4.png
   :align: center

5. You are now ready to install the Cyclone DDS CXX, click 'Install'.

.. image:: /_static/gettingstarted-figures/5.5-5.png
   :align: center

6. Click 'Finish'.

.. image:: /_static/gettingstarted-figures/5.5-6.png
   :align: center

The installation for Cyclone DDS CXX is complete, to build an
application using Cyclone DDS CXX, refer to `how to build your first
Cyclone DDS CXX
example <#building-your-first-cyclonedds-cxx-example>`__.

Test your CXX Installation for Native Installation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Environnement variable updates
''''''''''''''''''''''''''''''

To run an Eclipse Cyclone DDSCXX application, the required libraries
(such as ddsc.dll and ddscxx.dll) must be available in the executable
path. These libraries should be installed in the system default
location. However, if they are not installed there, the library search
path must be updated accordingly. On Linux use the command:

.. code-block:: PowerShell

    Set PATH=<cyclonedds-installation-location>\bin;<cyclonedds-cxx-installation-location>\bin

**Note:** Alternatively, copy the required libraries to the executables'
directory.

Running the pre-built example
'''''''''''''''''''''''''''''

A simple *Hello World* application is included in the Eclipse Cyclone
DDSCXX, it can be used to test the installation. The *Hello World*
application is located in: 

- **Windows:** ``<cyclonedds-cxx-directory>\build\bin\Debug`` 

- **Linux:** ``<cyclone-cxx-directory>/build/bin``

To run the example application, open two console windows, and navigate
to the appropriate directory. Run the ``ddscxxHelloworldPublisher`` in
one of the console windows by using the following command:

-  **Windows:** ``ddscxxHelloworldPublisher.exe``

-  **Linux:** ``./ddscxxHelloworldPublisher``

Run the ``ddscxxHelloworldSubscriber`` in the other console window
using:

-  **Windows:** ``ddscxxHelloworldSubscriber.exe``

-  **Linux:** ``./ddscxxHelloworldSubscriber.exe``

The output for the ``ddscxxHelloworldPublisher`` is as follows:

.. image:: /_static/gettingstarted-figures/5.6.2-1.png
   :align: center

The output for the ``ddscxxHelloworldSubscriber`` is as follows:

.. image:: /_static/gettingstarted-figures/5.6.2-2.png
   :align: center

For more information on how to build this application and the code which
has been used, refer to `Hello
World. <#building-your-first-cyclonedds-cxx-example>`__

Uninstalling Cyclone DDS CXX
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Uninstallation for Native Installation
''''''''''''''''''''''''''''''''''''''

At this stage, you can manually remove the install and build directory.
Alternatively, in Linux and macOS, in the install or build directory,
use the following command:

.. code-block:: bash

    rm -rf *

Uninstallation for product installer
''''''''''''''''''''''''''''''''''''

Windows
^^^^^^^

To uninstall the Cyclone DDS CXX either remove it from Programs and
Features in the Control Panel or use the Cyclone DDS CXX installer. In
the latest case start Cyclone DDS CXX package, and choose 'Remove'.

.. image:: /_static/gettingstarted-figures/5.7.2.1-1.png
   :align: center

Linux and macOS
^^^^^^^^^^^^^^^

TBD.

DDS CXX Hello World Code anatomy
================================

The previous chapter described the installation process that built
implicitly or explicitly the C++ *Hello World!* Example. The key concept
of DDS was introduced in Chapter 3. This chapter introduces the
structural code of a simple system made by an application that publishes
keyed messages and another one that subscribes and reads such data. Each
message represents a data object that is uniquely identified with a key
and a payload.

.. _key_steps_helloworld_cpp:

Keys steps to build the Hello World! application in CXX
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The *Hello World!* example has a very simple 'data layer' with a data
model made of one data type ``Msg`` who represents keyed messages (c,f
next subsection).

To exchange data, applications' business logic with Cyclone DDS must:

1. Declare its subscription and involvement into a **DDS domain**. A
   DDS domain is an administrative boundary that defines, scopes, and
   gathers all the DDS applications, data, and infrastructure that needs
   to interconnect and share the same data space. Each DDS domain has a
   unique identifier. Applications declare their participation within a
   DDS domain by creating a **Domain Participant entity**.
2. Create a **Data topic** with the data type described in the data
   model. The data types define the structure of the Topic. The Topic is,
   therefore, an association between the topic name and datatype. QoSs
   can be optionally added to this association. A Topic, therefore
   categorizes the data into logical classes and streams.
3. Create at least a **Publisher**, a **Subscriber**, and **Data
   Readers** and **Writers** object specific to the topic
   created earlier. Applications may want to change the default QoSs at
   this stage. In the Hello world! example, the ``ReliabilityQoS`` is
   changed from its default value (Best-effort) to Reliable.
4. Once the previous DDS computational objects are in place, the
   application logic can start writing or reading the data.

At the application level, readers and writers need not be aware of
each other. The reading application, now designated as application
Subscriber polls the data reader periodically until a writing
application, as a result of this called application Publisher, 
provides the required data into the shared Topic, namely ``HelloWorldData_Msg``.

The data type is described using the OMG `IDL
Language <http://www.omg.org/gettingstarted/omg_idl.htm>`__ located in
``HelloWorldData.idl`` file. This IDL file is considered the Data Model
of our example.

This data model is preprocessed and compiled by Cyclone-DDS CXX
IDL-Compiler to generate a CXX representation of the data as described
in Chapter 6. These generated source and header files are used by the
``HelloworldSubscriber.cpp`` and ``HelloworldPublisher.cpp``
application programs to share the *Hello World!* Message instance and
sample.

HelloWorld IDL
''''''''''''''

As explained in chapter 3, the benefits of using IDL language to define
data is to have a data model that is independent of the programming
languages. The ``HelloWorld.idl`` IDL file used in chapter 3 can
therefore be reused, it is compiled to be used within C++ DDS based
applications.

The HelloWorld data type is described in a language-independent way and
stored in the HelloWorldData.idl file (as in chapter 3).

.. code-block:: omg-idl

    module HelloWorldData
    {
        struct Msg
        {
            @key long userID;
            string message;
        };
    };

The OMG Interface Definition Language (IDL) subset uses a DDS
data definition language. In our simple example, the HelloWorld data
model is made of one module ``HelloWorldData``. A module can be seen 
as a namespace where data with interrelated semantics is represented 
together in the same logical set.

The struct Msg is the actual data structure that shapes the data used to
build the Topics. As already mentioned, a topic is an
association between a data type and a string name. The topic name is not
defined in the IDL file but is defined by the application business
logic at runtime.

In our case, the data type ``Msg`` contains two fields: ``userID`` and
``message`` payload. The ``userID`` is used to identify each message 
instance uniquely. This is done using the ``@key`` annotation.

The Cyclone DDS CXX IDL compiler translates the module names into
namespaces and structure name into classes.

It also generates code for public accessor functions for all fields
mentioned in the IDL struct, separate public constructors, and a
destructor:

-  A default (empty) constructor that recursively invokes the
   constructors of all fields
-  A copy-constructor that performs a deep copy from the existing class
-  A move-constructor that moves all arguments to its members

The destructor recursively releases all fields. It also generates code
for assignment operators that recursively construct all fields based on
the parameter class (copy and move versions). The following code snippet is
provided without warranty: the internal format can change, but the API 
it provides to your application code is stable.

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

**Note:** When translated into a different programming language, the
data has a different representation specific to the target
language. For instance, as shown in chapter 3, in C, the Helloworld data
type is represented by a C structure. This advantage of using a
neutral language like IDL to describe the data model. It can be
translated into different languages that can be shared between various
applications written in other programming languages.

The IDL compiler generated files
''''''''''''''''''''''''''''''''

The IDL compiler is a bison-based parser written in pure C and should be
fast and portable. It loads dynamic libraries to support different output
languages, but this is seldom relevant to you as a user. You can use
``CMake`` recipes as described above or invoke directly:

.. code-block:: bash

    idlc -l cxx HelloWorldData.idl

This results in the following new files that need to be compiled and
their associated object file linked with the Hello *World!* publisher
and subscriber application business logic:

-  ``HelloWorldData.hpp``
-  ``HelloWorldData.cpp``

When using CMake to build the application, this step is hidden and is
done automatically. For building with CMake, refer to `building the
HelloWorld example. <#build-the-dds-cxx-hello-world-example>`__

``HelloWorldData.hpp`` and ``HelloWorldData.cpp`` files contain the data
type of messages that are shared.

DDS CXX Hello World Business Logic
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As well as from the ``HelloWorldData`` data type files that the *DDS CXX
Hello World* example used to send messages, the *DDS CXX Hello World!*
the example also contains two application-level source files
(``subscriber.cpp`` and ``publisher.cpp``), containing the business
logic.

DDS CXX *Hello World* Subscriber Source Code
''''''''''''''''''''''''''''''''''''''''''''

The ``Subscriber.cpp`` file mainly contains the statements to wait for a
*Hello World* message and reads it when it receives it.

**Note:** The read sematic keeps the data sample in the Data Reader
cache. The Subscriber application implements the steps defined in
:ref:`Key Steps to build helloworld for C++ <key_steps_helloworld_cpp>`.

.. code-block:: C++
    :linenos:

    #include <cstdlib>
    #include <iostream>
    #include <chrono>
    #include <thread>

    /* Include the C++ DDS API. */
    #include "dds/dds.hpp"

    /* Include data type and specific traits to be used with the C++ DDS API. */
    #include "HelloWorldData.hpp"

    using namespace org::eclipse::cyclonedds;

    int main() {
        try {
            std::cout << "=== [Subscriber] Create reader." << std::endl;

            /* First, a domain participant is needed.
             * Create one on the default domain. */
            dds::domain::DomainParticipant participant(domain::default_id());

            /* To subscribe to something, a topic is needed. */
            dds::topic::Topic<HelloWorldData::Msg> topic(participant, "ddscxx_helloworld_example");

            /* A reader also needs a subscriber. */
            dds::sub::Subscriber subscriber(participant);

            /* Now, the reader can be created to subscribe to a HelloWorld message. */
            dds::sub::DataReader<HelloWorldData::Msg> reader(subscriber, topic);

            /* Poll until a message has been read.
             * It isn't really recommended to do this kind wait in a polling loop.
             * It's done here just to illustrate the easiest way to get data.
             * Please take a look at Listeners and WaitSets for much better
             * solutions, albeit somewhat more elaborate ones. */
            std::cout << "=== [Subscriber] Wait for message." << std::endl;
            bool poll = true;

            while (poll) {
                /* For this example, the reader will return a set of messages (aka
                 * Samples). There are other ways of getting samples from reader.
                 * See the various read() and take() functions that are present. */
                dds::sub::LoanedSamples<HelloWorldData::Msg> samples;

                /* Try taking samples from the reader. */
                samples = reader.take();

                /* Are samples read? */
                if (samples.length() > 0) {
                    /* Use an iterator to run over the set of samples. */
                    dds::sub::LoanedSamples<HelloWorldData::Msg>::const_iterator sample_iter;
                    for (sample_iter = samples.begin();
                         sample_iter < samples.end();
                         ++sample_iter) {
                        /* Get the message and sample information. */
                        const HelloWorldData::Msg& msg = sample_iter->data();
                        const dds::sub::SampleInfo& info = sample_iter->info();

                        /* Sometimes a sample is read, only to indicate a data
                         * state change (which can be found in the info). If
                         * that's the case, only the key value of the sample
                         * is set. The other data parts are not.
                         * Check if this sample has valid data. */
                        if (info.valid()) {
                            std::cout << "=== [Subscriber] Message received:" << std::endl;
                            std::cout << "    userID  : " << msg.userID() << std::endl;
                            std::cout << "    Message : \"" << msg.message() << "\"" << std::endl;

                            /* Only 1 message is expected in this example. */
                            poll = false;
                        }
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            }
        }
        catch (const dds::core::Exception& e) {
            std::cerr << "=== [Subscriber] Exception: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "=== [Subscriber] Done." << std::endl;

        return EXIT_SUCCESS;
    }

Within the subscriber code, we mainly use the DDS ISOCPP API and the
``HelloWorldData::Msg`` type. Therefore, the following header files must
be included:

-  The ``dds.hpp`` file give access to the DDS APIs,
-  The ``HelloWorldData.hpp`` is specific to the data type defined
   in the IDL.

.. code-block:: C++

    #include "dds/dds.hpp"
    #include "HelloWorldData.hpp"

At least four DDS entities are needed, the domain participant, 
the topic, the subscriber, and the reader.

.. code-block:: C++

    dds::domain::DomainParticipant participant(domain::default_id());
    dds::topic::Topic<HelloWorldData::Msg> topic(participant,"ddscxx_helloworld_example");
    dds::sub::Subscriber subscriber(participant);
    dds::sub::DataReader<HelloWorldData::Msg> reader(subscriber,topic);

The Cyclone DDS CXX API simplifies and extends how data can be read or
taken. To handle the data some, ``LoanedSamples`` is declared and
created which loans samples from the Service pool. Return of the loan is
implicit and managed by scoping:

.. code-block:: C++

    dds::sub::LoanedSamples<HelloWorldData::Msg> samples;
    dds::sub::LoanedSamples<HelloWorldData::Msg>::const_iterator sample_iter;

As the ``read( )/take()`` operation may return more the one data sample
(if several publishing applications are started
simultaneously to write different message instances), an iterator is
used.

.. code-block:: C++

    const::HelloWorldData::Msg& msg;
    const dds::sub::SampleInfo& info;

In DDS, data and metadata are propagated together. The samples are a 
set of data samples (i.e., user-defined data) and metadata describing the
sample state and validity, etc ,,, (``info``). We can use iterators to 
get the data and metadata from each sample.


.. code-block:: C++

    try {
        // ...
    }
    catch (const dds::core::Exception& e) {
        std::cerr << "=== [Subscriber] Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

It is good practice to surround every key verb of the DDS APIs with
``try/catch`` block to locate issues precisely when they occur. In this
example, one block is used to facilitate the programming model of the
applications and improve their source code readability.

.. code-block:: C++

    dds::domain::DomainParticipant participant(domain::default_id());

The DDS participant is always attached to a specific DDS domain. In the
Hello World! example, it is part of the Default\_Domain, the one
specified in the XML deployment file that you potentially be created
(i.e., the one pointing to ``$CYCLONEDDS_URI``), please refer to
:ref:`testing your installation <test_your_installation>` for further details.

Subsequently, create a subscriber attached to your participant.

.. code-block:: C++

    dds::sub::Subscriber subscriber(participant);

The next step is to create the topic with a given
name(\ ``ddscxx_helloworld_example``)and the predefined data
type(\ ``HelloWorldData::Msg``). Topics with the same data type
description and with different names are considered different topics.
This means that readers or writers created for a given topic do not
interfere with readers or writers created with another topic, even if
they are the same data type.

.. code-block:: C++

    dds::topic::Topic<HelloWorldData::Msg> topic(participant,"ddscxx_helloworld_example");

Once the topic is created, we can create and associate to it a data
reader.

.. code-block:: C++

    dds::sub::DataReader<HelloWorldData::Msg> reader(subscriber, topic);

To modify the Data Reader Default Reliability Qos to Reliable:

.. code-block:: C++

    dds::sub::qos::DataReaderQos drqos = topic.qos() << dds::core::policy::Reliability::Reliable();
    dds::sub::DataReader<HelloWorldData::Msg> dr(subscriber, topic, drqos);

To retrieve data in your application code from the data reader's cache
you can either use a pre-allocated buffer to store the data or loan it
from the middleware.

If you use a pre-allocated buffer, you create an array/vector-like 
like container. If you use the loaned buffer option, you need to be
aware that these buffers are actually 'owned' by the middleware,
precisely by the DataReader. The Cyclone DDS CXX API implicitly 
allows you to return the loans through scoping.


In our example, we use the loan samples mode (``LoanedSamples``).
``Samples`` are an unbounded sequence of samples; the sequence 
length depends on the amount of data available in the data reader's
cache.

.. code-block:: C++

    dds::sub::LoanedSamples<HelloWorldData::Msg> samples;

At this stage, we can attempt to read data by going into a polling loop
that regularly scrutinizes and examines the arrival of a message.
Samples are removed from the reader's cache when taken with the
``take()``.

.. code-block:: C++

    samples = reader.take();

If you choose to read the samples with ``read()``, data remains in the
data reader cache. A length() of samples greater than zero indicates
that the data reader cache was not empty.

.. code-block:: C++

    if (samples.length() > 0)

As sequences are NOT pre-allocated by the user, buffers are 'loaned' to
him by the DataReader.

.. code-block:: C++

    dds::sub::LoanedSamples<HelloWorldData::Msg>::const_iterator sample_iter;
    for (sample_iter = samples.begin();
         sample_iter < samples.end();
         ++sample_iter)

For each sample, cast and extract its user-defined data
(``Msg``) and metadate (``info``).

.. code-block:: C++

    const HelloWorldData::Msg& msg = sample_iter->data();
    const dds::sub::SampleInfo& info = sample_iter->info();

The SampleInfo (``info``) tells us whether the data we are taking is
*Valid* or *Invalid*. Valid data means that it contains the payload
provided by the publishing application. Invalid data means that we are
reading the DDS state of the data Instance. The state of a data instance can
be ``DISPOSED`` by the writer, or it is ``NOT_ALIVE`` anymore, which
could happen when the publisher application terminates while the
subscriber is still active. In this case, the sample is not considered
Valid, and its sample ``info.valid()`` field is False.

.. code-block:: C++

    if (info.valid())

As the sample contains valid data, we can safely display its content.

.. code-block:: C++

    std::cout << "=== [Subscriber] Message received:" << std::endl;
    std::cout << "    userID  : " << msg.userID() << std::endl;
    std::cout << "    Message : \"" << msg.message() << "\"" << std::endl;

As we are using the Poll data reading mode, we repeat the above steps
every 20 milliseconds.

.. code-block:: C++

    else {
          std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

This example uses the polling mode to read or take data. Cyclone DDS
offers *waitSet* and *Listener* mechanism to notify the application that
data is available in their cache, which avoids polling the cache at a
regular intervals. The discretion of these mechanisms is beyond the
scope of this document.

All the entities that are created under the participant, such as the
Data Reader Subscriber and Topic are automatically deleted by
middleware through the scoping mechanism.

DDS CXX *Hello World* Publisher Source Code
'''''''''''''''''''''''''''''''''''''''''''

The ``Publisher.cpp`` contains the source that writes a *Hello World*
message. From the DDS perspective, the publisher application code is
almost symmetrical to the subscriber one, except that you need to create
a Publisher and DataWriter, respectively, instead of a Subscriber and
Data Reader. A synchronization statement is added to the main thread to 
ensure data is only written when Cyclone DDS discovers at least a matching 
reader. Synchronizing the main thread until a reader is discovered
assures we can start the publisher or subscriber program in any order.

.. code-block:: C++
    :linenos:

    #include <cstdlib>
    #include <iostream>
    #include <chrono>
    #include <thread>

    /* Include the C++ DDS API. */
    #include "dds/dds.hpp"

    /* Include data type and specific traits to be used with the C++ DDS API. */
    #include "HelloWorldData.hpp"

    using namespace org::eclipse::cyclonedds;

    int main() {
        try {
            std::cout << "=== [Publisher] Create writer." << std::endl;

            /* First, a domain participant is needed.
             * Create one on the default domain. */
            dds::domain::DomainParticipant participant(domain::default_id());

            /* To publish something, a topic is needed. */
            dds::topic::Topic<HelloWorldData::Msg> topic(participant, "ddscxx_helloworld_example");

            /* A writer also needs a publisher. */
            dds::pub::Publisher publisher(participant);

            /* Now, the writer can be created to publish a HelloWorld message. */
            dds::pub::DataWriter<HelloWorldData::Msg> writer(publisher, topic);

            /* For this example, we'd like to have a subscriber read
             * our message. This is not always necessary. Also, the way it is
             * done here is to illustrate the easiest way to do so. It isn't
             * It is recommended to do a wait in a polling loop, however
             * Please take a look at Listeners and WaitSets for much better
             * solutions, albeit somewhat more elaborate ones. */
            std::cout << "=== [Publisher] Waiting for subscriber." << std::endl;
            while (writer.publication_matched_status().current_count() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            /* Create a message to write. */
            HelloWorldData::Msg msg(1, "Hello World");

            /* Write the message. */
            std::cout << "=== [Publisher] Write sample." << std::endl;
            writer.write(msg);

            /* With a normal configuration (see dds::pub::qos::DataWriterQos
             * for various writer configurations), deleting a writer will
             * dispose of all its related messages.
             * Wait for the subscriber to have stopped to be sure it received the
             * message. Again, not normally necessary and not recommended to do
             * this in a polling loop. */
            std::cout << "=== [Publisher] Waiting for sample to be accepted." << std::endl;
            while (writer.publication_matched_status().current_count() > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        catch (const dds::core::Exception& e) {
            std::cerr << "=== [Publisher] Exception: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "=== [Publisher] Done." << std::endl;

        return EXIT_SUCCESS;
    }

We are using the ISOCPP DDS API and the HelloWorldData to receive data.
For that, we need to include the appropriate header files.

.. code-block:: C++

    #include "dds/dds.hpp"
    #include "HelloWorldData.hpp"

An exception handling mechanism ``try/catch`` block is used.

.. code-block:: C++

    try {
        // …
    }
    catch (const dds::core::Exception& e) {
        std::cerr << "=== [Subscriber] Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

AS with the reader in ``subscriber.cpp``, we need a participant, a
topic, and a publisher to create a writer. We must also 

use the same topic name specified in the ``subscriber.cpp``.

.. code-block:: C++

    dds::domain::DomainParticipant participant(domain::default_id());
    dds::topic::Topic<HelloWorldData::Msg> topic(participant, "ddscxx_helloworld_example");
    dds::pub::Publisher publisher(participant);

With these entities ready, the writer can now be created. The writer is
created for a specific topic ``“ddscxx_helloworld_example”`` in the
default DDS domain.

.. code-block:: C++

    dds::pub::DataWriter<HelloWorldData::Msg> writer(publisher, topic);

To modify the DataWriter Default Reliability Qos to Reliable:

.. code-block:: C++

    dds::pub::qos::DataReaderQos dwqos = topic.qos() << dds::core::policy::Reliability::Reliable();
    dds::sub::DataWriter<HelloWorldData::Msg> dr(publisher, topic, dwqos);

When Cyclone DDS discovers readers and writers sharing the same data
type and topic name, it connects them without the application's
involvement. A rendezvous pattern is required to write data only when 
a data reader appears. Either can implement such a pattern:

1. Wait for the publication/subscription matched events, where the
   Publisher waits and blocks the writing thread until the appropriate
   publication matched event is raised, or
2. Regularly poll the publication matching status. This is the
   preferred option used in this example. The following line of code
   instructs Cyclone DDS to listen on the
   ``writer.publication_matched_status()``

.. code-block:: C++

    dds::pub::DataWriter<HelloWorldData::Msg> writer(publisher, topic);

At regular intervals, we get the status change and for a matching
publication. In between, the writing thread sleeps for 20 milliseconds.

.. code-block:: C++

    while (writer.publication_matched_status().current_count() == 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

After this loop, we are confident that a matching reader has been
discovered. Now, we can commence the writing of the data instance.
First, the data must be created and initialized

.. code-block:: C++

    HelloWorldData::Msg msg(1, "Hello World");

Send the data instance of the keyed message.

.. code-block:: C++

    writer.write(msg);

After writing the data to the writer, the *DDS CXX Hello World* example
checks if a matching subscriber(s) is still available. If 
a matching subscriber(s) exists, the example waits for 50ms and starts
publishing the data again. If no matching subscriber is found, then the
publisher program is ended.

.. code-block:: C++

    return EXIT_SUCCESS;

Through scoping, all the entities such as topic, writer, etc. are
deleted automatically.

Contributing to Eclipse Cyclone DDS
===================================

We welcome all contributions to the project, including questions,
examples, bug fixes, enhancements or improvements to the documentation,
etc.

If you want to contribute code, it is helpful to know that build
configurations for Azure DevOps Pipelines are present in the repositories.
There is a test suite using CTest and CUnit that can be built
locally. 

The following sections explain how to do this for the different
operating systems.

Linux and macOS
~~~~~~~~~~~~~~~

Set the CMake variable ``BUILD_TESTING`` to ``ON`` when configuring, e.g.:

.. code-block:: bash

    cd build
    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
    cmake --build .
    ctest

**Note:** To install the Cyclone DDS package:

.. code-block:: bash

    cmake -DCMAKE_INSTALL_PREFIX=<install-location> ..
    cmake --build . --target install

This build requires `CUnit <http://cunit.sourceforge.net/>`__. You can
install this yourself, or you can choose to instead rely on the
`Conan <https://conan.io/>`__ packaging system that the CI build
infrastructure also uses. In that case, install Conan in the build
directory before running CMake:

.. code-block:: bash

    conan install .. --build missing


Windows
~~~~~~~

Set the CMake variable ``BUILD_TESTING`` to ``ON`` when configuring, e.g.:

.. code-block:: bash

    cd build
    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
    cmake --build .
    ctest

**Note:** To install the Cyclone DDS package:

.. code-block:: bash

    cmake -DCMAKE_INSTALL_PREFIX=<install-location> ..
    cmake --build . --target install

This build requires `CUnit <http://cunit.sourceforge.net/>`__. You can
install this yourself, or you can choose to instead rely on the
`Conan <https://conan.io/>`__ packaging system that the CI build
infrastructure also uses. In that case, install Conan in the build
directory before running CMake:

.. code-block:: bash

    conan install .. --build missing

**Note:** depending on the generator, you may also need to add switches
to select the architecture and build type, e.g.:

.. code-block:: bash

    conan install -s arch=x86_64 -s build_type=Debug ..

This automatically downloads and builds CUnit
 (and currently OpenSSL for transport security).

Contributing to Eclipse Cyclone DDS CXX
=======================================

Linux and macOS
~~~~~~~~~~~~~~~

Set the CMake variable ``BUILD_TESTING`` to ``ON`` when configuring.

.. code-block:: bash

    cd build
    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
    cmake --build .
    ctest

**Note:** If CMake can not locate the Cyclone DDS:

.. code-block:: bash

    cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=<install-location> -DCMAKE_PREFIX_PATH="<cyclonedds-install-location>" -DBUILD_TESTING=ON ..

To install the package:

.. code-block:: bash

    cmake --build . --target install

This build requires `Google
Test <https://github.com/google/googletest>`__. You can install this
yourself or you can choose to instead rely on the
`Conan <https://conan.io/>`__ package manager that the CI build
infrastructure also uses. In that case, install Conan in the build
directory before running CMake:

.. code-block:: bash

    conan install .. --build missing

This automatically downloads and builds Google Test.

Windows
~~~~~~~

Set the CMake variable ``BUILD_TESTING`` to on when configuring.

.. code-block:: bash

    cd build
    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
    cmake --build .
    ctest

**Note:** If CMake can not locate the Cyclone DDS or IDL package:

.. code-block:: bash

    cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=<install-location> -DCMAKE_PREFIX_PATH="<cyclonedds-install-location>" -DBUILD_TESTING=ON ..

To install the package:

.. code-block:: bash

    cmake --build . --target install

This build requires `Google
Test <https://github.com/google/googletest>`__. You can install this
yourself or you can choose to instead rely on the
`Conan <https://conan.io/>`__ package manager that the CI build
infrastructure also uses. In that case, install Conan in the build
directory before running CMake:

.. code-block:: bash

    conan install .. --build missing

This automatically downloads and/or builds Google Test.

**Note:** Depending on the generator, you may also need to add switches
to select the architecture and build type, e.g.:


.. code-block:: bash

    conan install -s arch=x86_64 -s build_type=Debug ..

