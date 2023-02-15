.. include:: ../external-links.part.rst

.. index:: IDL; Python

.. _py_idl:

==========
Python IDL
==========

There is no official OMG specification for mapping IDL to Python. The solutions 
here are therefore not standardized and are not compatible with other DDS 
implementations. However, they are based purely on the standard library 
type-hinting functionality as introduced in Python 3.5, so that any Python tooling 
that works with type-hints is also compatible with our implementation. 

To generate initializers and string representations use the :py:mod:`dataclasses` 
standard library module. This is applied outside of the other IDL machinery, 
therefore you can control immutability, equality checking, or even use a different 
``dataclasses`` representation, for example |url::runtype_link|.

Working with the IDL compiler
-----------------------------

If you already have an IDL file that defines your types, or if you require 
interoperability with non-Python projects. The ``idlpy`` library is built as part of 
the python package. 

After installing the CycloneDDS Python backend you can run ``idlc`` with the ``-l py`` 
flag to generate Python code:

.. code-block:: shell

   idlc -l py your_file.idl

To nest the resulting Python module inside an existing package, you can specify the 
path from the intended root. If you have a package 'wubble' with a submodule 'fruzzy' 
and want the generated modules and types under there you can pass ``py-root-prefix``:

.. code-block:: shell

   idlc -l py -p py-root-prefix=wubble.fruzzy your_file.idl

IDL Datatypes in Python
-----------------------

The ``cyclonedds.idl`` package implements the IDL unions, structs, and their 
OMG XCDR-V1 encoding, in python. In most cases, the IDL compiler writes the code that 
references this package without the need to edit the objects. However, for python-only 
projects it is possible to write the objects manually (where cross-language 
interactions are not required). To manually write IDL objects, you can make your 
classes inherit from the following classes:

- :class:`IdlStruct<cyclonedds.idl.IdlStruct>`
- :class:`IdlStruct<cyclonedds.idl.IdlUnion>`
- :class:`IdlStruct<cyclonedds.idl.IdlEnum>`
- :class:`IdlStruct<cyclonedds.idl.IdlBitmaks>`

The following is a basic example of how to use dataclasses (For further information 
refer to the standard library documentation of :mod:`dataclasses<python:dataclasses>`):

.. code-block:: python
   :linenos:

   from dataclasses import dataclass
   from cyclonedds.idl import IdlStruct

   @dataclass
   class Point2D(IdlStruct):
      x: int
      y: int

   p1 = Point2D(20, -12)
   p2 = Point2D(x=12, y=-20)
   p1.x += 5

The :func:`dataclass<python:dataclasses.dataclass>` decorator turns a class with just 
names and types into a dataclass. The :class:`IdlStruct<cyclonedds.idl.IdlStruct>` 
parent class makes use of the type information defined in the dataclass to 
:ref:`(de)serialize messages<serialization>`. All normal dataclasses functionality is 
preserved, therefore to define default factories use :func:`field<python:dataclasses.field>` 
from the dataclasses module, or add a `__post_init__` method for more complicated 
construction scenarios.

Types
-----

Not all Python types are encodable with OMG XCDR-V1. Therefore, there are limitations 
to what you can put in an :class:`IdlStruct<cyclonedds.idl.IdlStruct>` class. The 
following is an exhaustive list of types:

Integers
^^^^^^^^

The default Python :class:`int<python:int>` type maps to an OMG XCDR-V1 64-bit integer. 
The :mod:`types<cyclonedds.idl.types>` module has all the other integers types that 
are supported in python.

.. code-block:: python
   :linenos:

   from dataclasses import dataclass
   from cyclonedds.idl import IdlStruct
   from cyclonedds.idl.types import int8, uint8, int16, uint16, int32, uint32, int64, uint64

   @dataclass
   class SmallPoint2D(IdlStruct):
      x: int8
      y: int8

.. note:: 
   These special types are just normal :class:`int<python:int>`s at runtime. They are 
   only used to indicate the serialization functionality what type to use on the 
   network. If you store a number that is not supported by that integer type you will 
   get an error during encoding. The ``int128`` and ``uint128`` are not supported.

Floats
^^^^^^

The Python :class:`float<python:float>` type maps to a 64-bit float, which is a 
`double` in C-style languages. The :mod:`types<cyclonedds.idl.types>` module has a 
``float32`` and ``float64`` type, ``float128`` is not supported.

Strings
^^^^^^^

The Python :class:`str<python:str>` type maps directly to the XCDR string. It is 
encoded with utf-8. Inside :mod:`types<cyclonedds.idl.types>` there is the 
:class:`bounded_str<cyclonedds.idl.types.bounded_str>` type for a string with maximum 
length.


.. code-block:: python
   :linenos:

   from dataclasses import dataclass
   from cyclonedds.idl import IdlStruct
   from cyclonedds.idl.types import bounded_str

   @dataclass
   class Textual(IdlStruct):
      x: str
      y: bounded_str[20]


Lists
^^^^^

The Python :func:`list<python:list>` is a versatile type. In normal python, a list 
is able to contain other types, but to be able to encode it, all of the contents 
must be the same type, and this type must be known beforehand. This can be achieved 
by using the :class:`sequence<cyclonedds.idl.types.sequence>` type.

.. code-block:: python
   :linenos:

   from dataclasses import dataclass
   from cyclonedds.idl import IdlStruct
   from cyclonedds.idl.types import sequence

   @dataclass
   class Names(IdlStruct):
      names: sequence[str]

   n = Names(names=["foo", "bar", "baz"])


In XCDR, this results in an 'unbounded sequence', which in most cases should be 
acceptable. However, use annotations to change to either:

- A 'bounded sequence'. For example, to limit the maximum allowed number of items.
- An 'array'. For example, if the length of the list is always the same.

.. code-block:: python
   :linenos:

   from dataclasses import dataclass
   from cyclonedds.idl import IdlStruct
   from cyclonedds.idl.types import sequence, array

   @dataclass
   class Numbers(IdlStruct):
      ThreeNumbers: array[int, 3]
      MaxFourNumbers: sequence[int, 4]


Dictionaries
^^^^^^^^^^^^

.. Note::
   Currently, dictionaries are not supported by the IDL compiler. However, if your 
   project is pure python there is no problem in using them.

Unlike the built-in Python :class:`dict<python:dict>` both the key and the value must 
have a constant type. To define a dictionary, use the :class:`Dict<python:typing.Dict>` 
from the :mod:`typing<python:typing>` module.

.. code-block:: python
   :linenos:

   from typing import Dict
   from dataclasses import dataclass
   from cyclonedds.idl import IdlStruct

   @dataclasses
   class ColourMap(IdlStruct):
      mapping: Dict[str, str]

   c = ColourMap({"red": "#ff0000", "blue": "#0000ff"})


Unions
^^^^^^

Unions in IDL are different to the unions defined in the :mod:`typing<python:typing>` 
module. IDL unions are *discriminated*, which means that they have a value that 
indicates which of the possibilities is active. 

To write discriminated unions, use the following:

-  :func:`@union<cyclonedds.idl.types.union>` decorator
-  :func:`case<cyclonedds.idl.types.case>` helper type.
-  :func:`default<cyclonedds.idl.types.default>` helper type. 

Write the class in a dataclass style, except only one of the values can be active 
at a time. The :func:`@union<cyclonedds.idl.types.union>` decorator takes one type 
as argument, which determines the type of what is differentiating the cases.

.. code-block:: python
   :linenos:

   from enum import Enum, auto
   from dataclasses import dataclass
   from cyclonedds.idl import IdlUnion, IdlStruct
   from cyclonedds.idl.types import uint8, union, case, default, MaxLen


   class Direction(Enum):
      North = auto()
      East = auto()
      South = auto()
      West = auto()


   class WalkInstruction(IdlUnion, discriminator=Direction):
      steps_n: case[Direction.North, int]
      steps_e: case[Direction.East, int]
      steps_s: case[Direction.South, int]
      steps_w: case[Direction.West, int]
      jumps: default[int]

   @dataclass
   class TreasureMap(IdlStruct):
      description: str
      steps: sequence[WalkInstruction, 20]


   map = TreasureMap(
      description="Find my Coins, Diamonds and other Riches!\nSigned\nCaptain Corsaro",
      steps=[
         WalkInstruction(steps_n=5),
         WalkInstruction(steps_e=3),
         WalkInstruction(jumps=1),
         WalkInstruction(steps_s=9)
      ]
   )

   print (map.steps[0].discriminator)  # You can always access the discriminator, which in this case would print 'Direction.North'


Objects
^^^^^^^

To reference other classes as member a type, use 
:class:`IdlStruct<cyclonedds.idl.IdlStruct>` or 
:class:`IdlUnion<cyclonedds.idl.IdlUnion>` classes that only contain serializable members. 

.. code-block:: python
   :linenos:

   from dataclasses import dataclass
   from cyclonedds.idl import IdlStruct
   from cyclonedds.idl.types import sequence

   @dataclass
   class Point2D(IdlStruct):
      x: int
      y: int

   @dataclass
   class Cloud(IdlStruct):
      points: sequence[Point]

.. _Serialization:

Serialization
^^^^^^^^^^^^^

Serialization and deserialization automatically occur within the backend. For debug 
purposes, or outside a DDS context it can be useful to look at the serialized data, 
or create Python objects from raw bytes. By inheriting from 
:class:`IdlStruct<cyclonedds.idl.IdlStruct>` or 
:class:`IdlUnion<cyclonedds.idl.IdlUnion>`, the defined classes automatically gain 
``instance.serialize() -> bytes`` and ``cls.deserialize(data: bytes) -> cls`` functions.

- Serialize is a member function that returns :class:`bytes<python:bytes>` with the 
  serialized object. 
- Deserialize is a :func:`classmethod<python:classmethod>` that takes the 
  :class:`bytes<python:bytes>` and returns the resultant object. 
  
  To inspect the member types, use the built-in Python ``cls.__annotations__``, and 
  for for idl information, use the ``cls.__idl_annotations__`` and 
  ``cls.__idl_field_annotations__``.

.. code-block:: python
   :linenos:

   from dataclasses import dataclass
   from cyclonedds.idl import IdlStruct

   @dataclass
   class Point2D(IdlStruct):
      x: int
      y: int

   p = Point2D(10, 10)
   data = p.serialize()
   q = Point2D.deserialize(data)

   assert p == q


Idl Annotations
^^^^^^^^^^^^^^^

In IDL you can annotate structs and members with several different annotations, for 
example ``@key``. In Python we have decorators, but they only apply to classes not to 
fields. This is the reason why the syntax in Python for a class or field annotation 
differ slightly. Note: The IDL ``#pragma keylist`` is a class annotation in python, 
but functions in exactly the same way.

.. code-block:: python
   :linenos:

   from dataclasses import dataclass
   from cyclonedds.idl import IdlStruct
   from cyclonedds.idl.annotations import key, keylist

   @dataclass
   class Type1(IdlStruct):
      id: int
      key(id)
      value: str

   @dataclass
   @keylist(["id"])
   class Type2(IdlStruct):
      id: int
      value: str


.. _runtype: https://pypi.org/project/runtype/