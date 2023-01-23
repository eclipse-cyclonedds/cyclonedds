.. index:: ! HelloWorld
    single: HelloWorld; Running

.. _helloworld_main:

###################
HelloWorld tutorial
###################

.. toctree::
    :maxdepth: 1
    :hidden:

    helloworld_building
    helloworld_key_steps
    helloworld_idl
    helloworld_source_code
    helloworld_run

HelloWorld is a simple application that introduces the fundamental concept of DDS. 
The applications publish keyed messages, and another subscribes and reads the data. 
Each message represents a data object that is uniquely identified with a unique 
key and a payload.

Default directory
=================
The HelloWorld executables are located in:

- Windows: ``<cyclonedds-directory>\build\bin\Debug`` 
- Linux/macOS: ``<cyclonedds-directory>/build/bin`` 
