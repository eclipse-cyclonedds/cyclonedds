# Visitor API for IDL back-end

An IDL back-end generates program code for a certain target language.
In doing so, it often follows the global structure of the IDL specification,
rearraging the order of the IDL elements only when this is strictly required.
It is possible that for some target language multiple files are generated
from the IDL specification. Think for example about C(++) where a header
and an implementation file could be needed or about Java where each (public)
class requires a separate file.

The purpose of the IDL compiler API is to facilitate the efficient development
of back-ends. There are various ways to design such an API. There are no
strong reasons why the API should hide the internal data structures which
are used to represent the IDL type definitions. At the moment there are
two possible front-ends: one that parses IDL files and one that interprets
the type descriptions that are communicated between nodes. Besides back-ends
that generate code for various languages there might also be a back-end which
produces the type descriptions that are communicated between nodes.

A possible choice for the IDL back-end API, is to use the visitor pattern.
With this pattern, one creates a visitor that walks over the internal data
structure and calls the appropriate call-back functions. There is no need to
hide the structure of the internal data structure, meaning that the call-back
function can explore the data structure. Of course, the visitor should be
implemented such that this is avoided as much as possible. For example, there
could be a need to adjust the order in which the internal data structure is
traversed, by means of some switch. For example, for a back-end that does not
support nested structs, there could be de-nesting switch, which instructs the
visitor to first visit nested structs, before visiting the structs in which
they appear. It is also possible to support this, by having a special
call-back function, when present, will force the visitor to walk in the
de-nested order.

There are also some cases, where a certain part of the internal data structure
needs to be traversed several times with different call-back functions. An
example of this is generating code for a struct where the members of the
struct needs to be traversed for generating the class definition, the constructor
method and the destructor. There are several solutions for dealing with this.

One way of doing this, and kind of breaking the visitor pattern, is to
allow a nested invocation of the visitor within the call-back function. Then
it would be possible to temporarily override some of the call-back functions
and repeat this a number of times. (Of course, after leaving the call-back
function, the visitor should be notified to skip the underlying elements.)

Usually when one has to generate code for a sequence of elements, one has
to generate some code for the start, some code for each element, and some
code for the end. This results in three call-back functions. In case this
has to be repeated for a certain element (such as for the members of a struct),
it could be sufficient to have a mechanism to register a sequence of tupples
of the three call-back functions. If such a mechanism needs to be implemented
for each kind of element or a specific subset of element, is something that
needs to be investigated, by studying the patterns of code fragments that
occur in popular back-ends.

## Internal data structures

Because the back-ends will have access to the internal data structures to
represent the IDL type definitions, it is important that those data
structures are well documented and stable.

The internal data structure will have the form of an Abstract Syntrax Tree,
which closely follows the IDL syntax, this because the order of the various
elements is important. Here we do only describe the global design of the
most important types.

### Generic node

The generic node serves as a basis for all other elments for the internal
representation of IDL types. It has the following members:
* parent: a pointer to its parent node.
* children: a pointer to it next child.
* next: a pointer to the next sibling, the next child of the parent node.

### Type specifications

These are nodes that represent a type specification. These include the
base and template type specifications. The template types include:
string, wide string, sequence, fixed point, and map. 

### Type definitions

Type definitions have a name and usually also open a new scope for its
elements. The type definition adds a name to the generic node. These
include: module, struct, union, const, typedef, native, bitset, and
bitmask.

