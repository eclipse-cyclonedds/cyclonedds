.. include:: ../external-links.part.rst

.. index:: IDL; syntax format

.. _idl_syntax:

##########
IDL Syntax
##########

|var-project| follows the |url::omg_idl_4.2| specification for its IDL syntax.

.. _idl_comment_syntax:

**************
Comment Syntax
**************

IDL supports single-line and multi-line comments in a similar fashion to C/C++.

Single-line comments start with `//` and continue to the end of the line.

Multi-line comments are enclosed in `/*` and `*/`.

.. code-block:: shell

  // This is a single-line comment
  /* This is a multi-line comment
    that spans multiple lines */


.. _idl_include_file:

********************
Including Other IDLs
********************

You can include other IDL files using the `#include` directive. This allows you to reuse type definitions across multiple IDL files.

.. code-block:: shell

   #include "OtherFile.idl"

.. _idl_primitive_types:

***************
Primitive Types
***************

The table below shows the primitive types supported by the IDL compiler for both C/C++ and Python. Some aliases are defined, but only for primitive types

.. list-table::
  :align: left
  :header-rows: 1
  :widths: 20 20 20 40

  * - IDL Type
    - Aliases
    - C/C++11 Types
    - Description
  * - ``boolean``
    - ``n/a``
    - ``bool``
    - Boolean type, true or false
  * - ``char``
    - ``n/a``
    - ``char``
    - 8-bit character
  * - ``octet``
    - ``uint8``
    - ``uint8_t``
    - 8-bit unsigned integer
  * - ``short``
    - ``int16``
    - ``int16_t``
    - 16-bit unsigned integer
  * - ``unsigned short``
    - ``uint16``
    - ``uint16_t``
    - 16-bit unsigned integer
  * - ``long``
    - ``int32``
    - ``int32_t``
    - 32-bit signed integer
  * - ``unsigned long``
    - ``uint32``
    - ``uint32_t``
    - 32-bit unsigned integer
  * - ``long long``
    - ``int64``
    - ``int64_t``
    - 64-bit signed integer
  * - ``unsigned long long``
    - ``uint64``
    - ``uint64_t``
    - 64-bit unsigned integer
  * - ``float``
    - ``n/a``
    - ``float``
    - 32-bit floating point number
  * - ``double``
    - ``n/a``
    - ``double``
    - 64-bit floating point number
  * - ``string``
    - ``n/a``
    - ``std::string``
    - UTF-8 encoded string

.. _idl_arrays:

******
Arrays
******

Arrays in IDL are defined using the syntax `type name[size]`, where `type` is the element type, `name` is the name of the variable, and `size` is the number of elements. For dynamically sized arrarys, see :ref:`idl_sequences`.

.. list-table::
  :align: left
  :header-rows: 1
  :widths: 50 50

  * - IDL Type
    - C/C++11 Types
  * - ``char a[5]``
    - ``std::array<char, 5> a``
  * - ``octet a[5]``
    - ``std::array<uint8_t, 5> a``
  * - ``short a[5]``
    - ``std::array<int16_t, 5> a``
  * - ``unsigned short a[5]``
    - ``std::array<uint16_t, 5> a``
  * - ``long a[5]``
    - ``std::array<int32_t, 5> a``
  * - ``unsigned long a[5]``
    - ``std::array<uint32_t, 5> a``
  * - ``long long a[5]``
    - ``std::array<int64_t, 5> a``
  * - ``unsigned long long a[5]``
    - ``std::array<uint64_t, 5> a``
  * - ``float a[5]``
    - ``std::array<float, 5> a``
  * - ``double a[5]``
    - ``std::array<double, 5> a``

.. _idl_sequences:

*********
Sequences
*********

Sequences in IDL are mapped to an `std::vector` like structure. They can grow and shrink dynamically, and their size is not fixed at compile time.

.. list-table::
  :align: left
  :header-rows: 1
  :widths: 50 50

  * - IDL Type
    - C/C++11 Types
  * - ``boolean``
    - ``bool``
  * - ``sequence<char>``
    - ``std::vector<char>``
  * - ``sequence<octet>``
    - ``std::vector<uint8_t>``
  * - ``sequence<short>``
    - ``std::vector<int16_t>``
  * - ``sequence<unsigned short>``
    - ``std::vector<uint16_t>``
  * - ``sequence<long>``
    - ``std::vector<int32_t>``
  * - ``sequence<unsigned long>``
    - ``std::vector<uint32_t>``
  * - ``sequence<long long>``
    - ``std::vector<int64_t>``
  * - ``sequence<unsigned long long>``
    - ``std::vector<uint64_t>``
  * - ``sequence<float>``
    - ``std::vector<float>``
  * - ``sequence<double>``
    - ``std::vector<double>``

.. _idl_maps:

****
Maps
****

Maps in IDL are defined using the `map<key_type, value_type>` syntax, where `key_type` is the type of the keys and `value_type` is the type of the values. They are typically implemented as a `std::map` in C++.

.. list-table::
  :align: left
  :header-rows: 1
  :widths: 50 50

  * - IDL Type
    - C/C++11 Types
  * - ``map<char, long>``
    - ``std::map<char, int64_t>``

.. _idl_enumerations:

************
Enumerations
************

Enumerations are typically used to define a set of named constants. In IDL, it is defined using the `enum` keyword followed by the name and a list of enumerators, and is mapped directly to a C++ enum.

The following IDL definition:

.. code-block:: shell

  enum Color
  {
    RED,
    YELLOW,
    @value(3)
    BLUE
  };

Would be converted to the following C++ code:

.. code-block:: C++

  enum class Color
  {
    RED,
    YELLOW,
    BLUE = 3
  };

.. _idl_bitmasks:

********
Bitmasks
********

Bitmasks in IDL are a special kind of enumeration defined using the `bitmask` keyword followed by the name and the underlying type. They are typically used to represent a set of flags in binary form.

For example, the following IDL definition:

.. code-block:: shell

  @bit_bound(8)
  bitmask MyFlags : unsigned long
  {
    FLAG_ONE = 0x1,
    FLAG_TWO = 0x2,
    FLAG_THREE = 0x4
  };

Would be converted to the following C++ code:

.. code-block:: C++

  enum class MyFlags : uint32_t
  {
    FLAG_ONE = 0x1,
    FLAG_TWO = 0x2,
    FLAG_THREE = 0x4
  };
