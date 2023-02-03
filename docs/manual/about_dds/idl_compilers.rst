.. index:: 
    single: IDL; Compilers
    single: Compilers; IDL

.. _idl_compiler:

IDL compiler
============

The |var-project-short| IDL compiler translates module names into namespaces 
and structure names into classes.

It also generates code for public accessor functions for all fields mentioned in 
the IDL struct, separate public constructors, and a destructor:

- A default (empty) constructor that recursively invokes the constructors of 
  all fields.
- A copy-constructor that performs a deep copy from the existing class.
- A move-constructor that moves all arguments to its members.

The destructor recursively releases all fields. It also generates code for 
assignment operators that recursively construct all fields based on the 
parameter class (copy and move versions).

.. note::

    When translated into a different programming language, the data has another 
    representation specific to the target language. This highlights the advantage 
    of using a neutral language such as IDL to describe the data model. It can be
    translated into different languages that can be shared between different
    applications written in other programming languages.

