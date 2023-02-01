..
   Copyright(c) 2021 ZettaScale Technology and others

   This program and the accompanying materials are made available under the
   terms of the Eclipse Public License v. 2.0 which is available at
   http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
   v. 1.0 which is available at
   http://www.eclipse.org/org/documents/edl-v10.php.

   SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

.. index:: Custom containers (C++)

.. _cpp_custom_containers:

C++ custom containers
=====================

IDLCXX enables users to supply their own container classes, replacing the default containers
used for IDL sequences (bounded and unbounded), strings (bounded and unbounded), arrays and
unions. All custom containers templates are set through the ``-f`` command line option, followed
by the option to be set, an equals sign and the value to set it to. Each template can have a
varying number of tags, which are indicated by the following strings:

- ``{TYPE}``: will replace type names in sequences and arrays

- ``{BOUND}``: will replace maximum size values in bounded sequences and strings

- ``{DIMENSION}``: will replace size in arrays

For example, to set a custom container for the bounded sequence, you must add the following
to the command line options of ``idlc``:

.. code:: bash

  -f bounded-sequence-template="company_name::special_bounded_sequence_impl<{TYPE}, {BOUND}>"

Supplying this command line option converts the following IDL code from:

.. code:: IDL

  sequence<long,255>

to the following C++ code:

.. code:: C++

  company_name::special_bounded_sequence_impl<uint32_t, 255>

For each template, the ordering of the tags is not important, the generator will replace based
on tags, not positions, all other text is inserted into the generated code verbatim. This enables
custom allocators to also be added.

For more information on all command line options, run ``idlc`` with the ``-h`` option, while loading 
the idlcxx library. For example:

.. code:: bash

  idlc -l libdidlcxx.so -h

Sequences
---------

The default container for both bounded and unbounded IDL sequences is ``std::vector``.
The custom containers for bounded and unbounded sequences can be defined independently.
Any user-supplied container class that replaces ``std::vector`` must comply
with the following interface (with the same effect as these functions in std::vector):

- ``size``

- ``resize``

- ``data``

- ``operator==``

- ``operator[]``

- ``copy assignment operator``

- ``copy constructor``

The command line options for custom sequence containers are:

- ``bounded-sequence-template``

  - template used for bounded sequences instead of ``std::vector``

  - replaced tags: ``{TYPE}``, ``{BOUND}``

- ``bounded-sequence-include``

  - header to include if ``bounded-sequence-template`` is used

- ``sequence-template``

  - template used for sequences instead of ``std::vector``

  - replaced tags: ``{TYPE}``

- ``sequence-include``

  - header to include if ``sequence-template`` is used

Strings
-------

The default container used for both bounded and unbounded IDL strings is ``std::string``.
The custom containers for bounded and unbounded strings can be defined independently.
Any user-supplied container class that replaces ``std::string`` must comply
with the following interface (with the same effect as these functions in ``std::string``):

- ``length``

- ``assign``

  - the variant taking an input pointer and a length

- ``operator==``

- ``copy assignment operator``

- ``copy constructor``

The command line options for custom string containers are:

- ``bounded-string-template``

  - template to use for strings instead of ``std::string``

  - replaced tags: ``{BOUND}``

- ``bounded-string-include``

  - header to include if ``bounded-string-template`` is used

- ``string-template``

  - template to use for strings instead of ``std::string``

  - replaced tags: none

- ``string-include``

  - header to include if ``string-template`` is used

Arrays
------

The default container used for IDL arrays is ``std::array``.
Any user-supplied container class that replaces ``std::array`` must comply
with the following interface (with the same effect as these functions in ``std::array``):

- support auto-range for loops

  - having begin() and end() functions returning iterators to the begin and end of the array

- the function data()

The command line options for custom array containers are:

- ``array-template``

  - template to use for arrays instead of ``std::array``

  - replaced tags: ``{TYPE}``, ``{DIMENSION}``

- ``array-include``

  - header to include if template for ``array-template`` is used

Unions
------

IDL unions use the ``std::variant`` class by default as the container for the union values.
The only function needed from the custom union container is the templated getter function:

.. code:: c++

  template<typename T>T get(variant& var)

The command line options for custom variant containers are:

- ``union-getter-template``

  - template to use for reading the value of a variant, copied verbatim

- ``union-template``

  - template to use for unions instead of ``std::variant``, copied verbatim

- ``union-include``

  - header to include if template for union-template is used
