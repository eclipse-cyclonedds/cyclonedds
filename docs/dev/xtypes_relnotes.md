# Cyclone XTypes support

## Release 0.9

### Type System

- The following data types are not supported: map, bitset, wide-strings, char16, float128
- For the C language binding, additionally the following types are not supported as part of a type’s key: union, sequence
- Union types:
    - Using bitmask type as discriminator is not supported
    - Inheritance (7.2.2.4.5) is not supported
    - Extensibility `mutable` for unions is not supported
- The Dynamic Language Binding (7.5.2) is not supported (7.6.6, DynamicData and DynamicType API). Note: the Python API supports dynamic types without requiring a separate API.
- The built-in TypeLookup service (7.6.3.3) has no support for requesting type dependencies (service operation `getTypeDependencies`, section 7.6.3.3.4.1) and replying to a request of this type.
    - Because of this, handling `PublicationBuiltinTopicData` or `SubscriptionBuiltinTopicData` with an incomplete set of dependent types (i.e. number of entries in `dependent_typeids` is less than `dependent_typeid_count`) may result in a failure to match a reader with a writer.
- In case a union has a default case, the C (de)serializer requires that the default case comes last because of a limitation of the IDL compiler.
- Using the `try_construct` annotation (7.2.2.7) with a parameter other than `DISCARD` (the default) is not supported.
- The C deserializer does not support explicit defaults for members of an aggregated type (`default` annotation)
- External (7.3.1.2.1.4) collections element types not supported (e.g. `sequence<@external b>`)
- Using `default_literal` (7.3.1.2.1.10) to set the default for enumerated types is not supported
- Default extensibility is `final` rather than `appendable` to maintain backwards compatibility with DDS implementations that do not support XTypes (including Cyclone DDS versions prior to 0.9.0). The IDL compiler has command-line option to select a different default.

### Type Representation

- Type Object type representation
    - Recursive types are not supported (Strongly Connected Components, 7.3.4.9)
    - User-defined annotations (7.3.1.2.4) and `verbatim` annotations (7.3.2.5.1.1) are not included in complete type objects
- IDL type representation
    - Pragma declarations other than `keylist` are not supported
    - Alternative Annotation Syntax (7.3.1.2.3) is not supported
    - `verbatim` annotation (7.3.2.5.1.1) is not supported
    - `ignore_literal_names` annotation (7.3.1.2.1.11) is not supported
    - `non_serialized` annotation (7.3.1.2.1.14) is not supported
- XML (7.3.2) and XSD (7.3.3) type representation not supported

### Data Representation

- Default data representation is XCDR1 for `@final` types without optional members to maintain backwards compatibility with DDS implementations that do not support XTypes (including Cyclone DDS versions prior to 0.9.0).
    
    All other types require XCDR2: following 7.6.3.1.1 there is no need to support XCDR1 for interoperating with DDS implementations (ignoring those that only support XTypes 1.0 or 1.1, but not 1.2 or later).
    
    The C serializer does not support PL-CDR version 1 nor optional members in PLAIN-CDR version 1.
    
- XML data representation (7.4.4) is not supported