# Type support

[DDS](https://www.omg.org/spec/DDS/) is data-centric. To communicate between
applications, possibly written in different languages, either locally or
remotely, the middleware must marshal the data to a Common Data Representation
(CDR) suitable for transmission over a network.

To marshal the data, the middleware must know how it is constructed. To achieve
this, types communicated between applications are defined in the Interface
Definition Language (IDL). The IDL type definitions are processed by an IDL
compiler to generate language specific type definitions and
instructions/routines to marshal in-memory data to/from CDR.

> The *Interface Definition Language* (IDL) and *Common Data Representation*
> (CDR) are part of the *[CORBA](https://www.omg.org/spec/CORBA/)*
> specification.


## Runtime type generation

Interpreted languages, or languages that are not compiled to machine-language
instructions often allow type introspection (e.g. Java) or even reflection
(e.g. Python).

With the introduction of (Extensible and Dynamic Topic Types for DDS)
[https://www.omg.org/spec/DDS-XTypes] \(XTypes\) type descriptions must be
communicated with the topics so that applications can verify type compatibility
and/or type assignability.

The availability of all information required to (re)construct types from the
topic description makes it possible to construct language native types at
runtime, i.e. without the need for IDL type definitions being available at
compile time. For languages that support introspection or reflection, this
allows types to be introduced dynamically. Of course, if a language supports
introspection or reflection it is also possible to introduce types into the
system dynamically.


## TypeTree

Type generation at compile time and runtime share a lot of commonalities. As
such, if cleverly constructed, it is expected that various parts can simply be
shared between the two.

As is customary in compiler design, the input must first be converted into a
representation suitable for traversal in memory. This part of the process is
handled by the *frontend*. The *frontent* takes a type definition, or set of
type definitions, verify they are correct and store them in memory in an
intermediate representation that is understood by the *backend*. The *backend*
takes the intermediate format and uses it to generate one or more target
representations. e.g. For native C types to be used, the *backend* would need
to generate native C language representations (opcodes for the serialization
VM included), the *TypeObject* representation for XTypes compatibility and
the OpenSplice XML format for comatibility with OpenSplice.

    --------                                -----
    |  IDL |---------\                  /-- | C |
    --------         |                  |   -----
                     |                  |   -------
                     |                  |   | C++ |
                     |                  |   -------
    --------------   |   ------------   |   --------------
    | TypeObject | -->-- | TypeTree | -->-- | TypeObject |
    --------------   |   ------------   |   --------------
                     |                  |   -------
                     |                  |-- | XML |
                     |                  |   -------
    -------          |                  |   ------------------
    | XML |----------/                  \-- | OpenSplice XML |
    -------                                 ------------------

The diagram above provides a very minimal overview of the different parts
involved with translating language agnostic type definitions into language
native type definitions. The diagram clearly shows the importance of the
intermediate format, hereafter to be referred to as the *TypeTree*.


### TypeTree design

A backend, in essence, does nothing more than generate a native language
representation of the TypeTree. But, the output and the internal flow vary
greatly between languages.

For instance, C++ requires only a header and a source file. Java, however,
requires each (public) class to reside in a separate file.

Previous incarnations of IDL compilers studied often used a visitor pattern.
i.e. The backend registers a set of callback functions, usually one or more
for each type, and the programmer calls a function that *visits* each type
exactly once and calls the appropriate callback functions in order. However,
this method is considered to rigid.

1. There are cases where a part of the tree must be traversed several times
   with different callback functions. For instance, when generating code for
   for a struct in an object oriented language, all members must be traversed
   before the class definition, constructors and destructors can be generated.

2. There could be a need to adjust the order in which the tree is traversed.
   For instance when a given language does not support nested structs. The
   order in which the tree is traversed must be reversed to first emit the
   definitions of the nested structs before the definition of the current
   struct can be emitted.

3. ...

One of the design goals of the intermediate format and accompanying utilities
is to facilitate efficient development of backends. Therefore, because there
is no single pattern that works well for all supported languages, the TypeTree
itself will be considered the API and basic utility functions will be offered
to simplify traversing the TypeTree etc.


### TypeTree structure

The TypeTree, as indicated by the name, is a simple tree that closely follows
IDL syntax. This is a requirement because the order has importance. A
pseudocode example of how types are constructed is provided below.

```C
#define INT (1<<0)
// ...
#define TYPEDEF (1<<10)
#define STRUCT (1<<11)
#define MAP (1<<12)
// ...
#define ARRAY (1<<20)
#define UNBOUND (1<<21)

typedef union type type_t;

typedef void(*dtor_t)(type_t *type, type_t *parent);

typedef struct {
  int flags;
  char *name;
  type_t *next;
  type_t *parent;
  dtor_t dtor;
} typespec_t;

typedef struct {
  typespec_t type; /* ARRAY bit set icw. normal type flags and perhaps UNBOUND */
  size_t size;
} array_t;

typedef struct { /* TYPEDEF bit set, name is name of typedef. */
  typespec_t type;
  typespec_t *target; /* Pointer to target type. */
} typedef_t;

typedef struct {
  typespec_t type; /* FORWARD_DECL and type bits set. */
  type_t *target;
} forward_decl_t;

/* Constructed types introduce a complexity when the TypeTree is destructed
   because they can be both embedded and referenced. A constructed type must
   only be destructed from the scope in which it is defined, never when merely
   referenced. */
typedef struct {
  typespec_t type;
  type_t *members;
} struct_t;

typedef struct {
  typespec_t type;
  type_t *key;
  type_t *value;
} map_t;

typedef struct {
  typespec_t type;
  type_t *elem;
} sequence_t;

/* Allows dereference without a cast. e.g. type->tu_array.size. */
union type {
  typespec_t tu_spec;
  array_t tu_array;
  typedef_t tu_typedef;
  struct_t tu_struct;
  map_t tu_map;
};
```

