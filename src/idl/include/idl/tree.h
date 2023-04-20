// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDL_TREE_H
#define IDL_TREE_H

#include <stdbool.h>
#include <stdint.h>

#include "idl/export.h"
#include "idl/retcode.h"
#include "idl/symbol.h"

/* the parser constructs a tree representing the idl document from
   specialized nodes. each node is derived from the same base node, which
   contains properties common accross nodes, and is either a declaration,
   specifier, expression, constant, pragma, or combination thereof. constants
   contain the result from an expression, pragmas contain compiler-specific
   instructions that apply to a specific declaration, much like annotations.
   the exact type of a node is stored in the mask property of the base node
   and is an constructed by combining preprocessor defines. unique bits are
   reserved for categories and properties that generators are likely to filter
   on when applying a visitor pattern. */

#define IDL_DECLARATION \
  (IDL_MODULE | IDL_CONST | IDL_CONSTR_TYPE | IDL_TYPEDEF | \
   IDL_MEMBER | IDL_CASE | IDL_CASE_LABEL | IDL_ENUMERATOR | IDL_DECLARATOR | IDL_BIT_VALUE)

#define IDL_TYPE \
  (IDL_BASE_TYPE | IDL_TEMPL_TYPE | IDL_CONSTR_TYPE | IDL_TYPEDEF)

#define IDL_CONSTR_TYPE \
  (IDL_STRUCT | IDL_UNION | IDL_ENUM | IDL_BITMASK)

#define IDL_TEMPL_TYPE \
  (IDL_SEQUENCE | IDL_STRING | IDL_WSTRING | IDL_FIXED_PT)

/* miscellaneous */
#define IDL_KEYLIST (1llu<<39)
#define IDL_KEY (1llu<<38)
#define IDL_INHERIT_SPEC (1llu<<37)
#define IDL_SWITCH_TYPE_SPEC (1llu<<36)
#define IDL_LITERAL (1ull<<35)
/* declarations */
#define IDL_MODULE (1llu<<34)
#define IDL_CONST (1llu<<33)
#define IDL_MEMBER (1llu<<32)
#define IDL_FORWARD (1llu<<31)
#define IDL_CASE (1llu<<30)
#define IDL_CASE_LABEL (1llu<<29)
/* if explicit default is specified */
#define IDL_DEFAULT_CASE_LABEL (IDL_CASE_LABEL | 1u)
/* if no explicit default is specified and range is not covered */
#define IDL_IMPLICIT_DEFAULT_CASE_LABEL (IDL_DEFAULT_CASE_LABEL | 2u)
#define IDL_ENUMERATOR (1llu<<28)
#define IDL_DEFAULT_ENUMERATOR (IDL_ENUMERATOR | 1u)
#define IDL_IMPLICIT_DEFAULT_ENUMERATOR (IDL_DEFAULT_ENUMERATOR | 2u)
#define IDL_BIT_VALUE (1llu<<27)
#define IDL_DECLARATOR (1llu<<26)
/* annotations */
#define IDL_ANNOTATION (1llu<<25)
#define IDL_ANNOTATION_MEMBER (1llu<<24)
#define IDL_ANNOTATION_APPL (1llu<<23)
#define IDL_ANNOTATION_APPL_PARAM (1llu<<22)

/* bits 20 - 21 are reserved for operators (not exposed in tree) */

typedef enum idl_type idl_type_t;
enum idl_type {
  IDL_NULL = 0u,
  IDL_TYPEDEF = (1llu<<19),
  /* constructed types */
  IDL_STRUCT = (1u<<17),
  IDL_UNION = (1u<<16),
  IDL_ENUM = (1u<<15),
  IDL_BITMASK = (1u<<18),
  /* template types */
  IDL_SEQUENCE = (1llu<<14),
  IDL_STRING = (1llu<<13),
  IDL_WSTRING = (1llu<<12),
  IDL_FIXED_PT = (1llu<<11),
  /* simple types */
  /* miscellaneous base types */
#define IDL_BASE_TYPE (1llu<<10)
#define IDL_UNSIGNED (1llu<<0)
  IDL_CHAR = (IDL_BASE_TYPE | (1u<<1)),
  IDL_WCHAR = (IDL_BASE_TYPE | (2u<<1)),
  IDL_BOOL = (IDL_BASE_TYPE | (3u<<1)),
  IDL_OCTET = (IDL_BASE_TYPE | (4u<<1) | IDL_UNSIGNED),
  IDL_ANY = (IDL_BASE_TYPE | (5u<<1)),
  /* integer types */
#define IDL_INTEGER_TYPE (1llu<<9)
  IDL_SHORT = (IDL_BASE_TYPE | IDL_INTEGER_TYPE | (1u<<1)),
  IDL_USHORT = (IDL_SHORT | IDL_UNSIGNED),
  IDL_LONG = (IDL_BASE_TYPE | IDL_INTEGER_TYPE | (2u<<1)),
  IDL_ULONG = (IDL_LONG | IDL_UNSIGNED),
  IDL_LLONG = (IDL_BASE_TYPE | IDL_INTEGER_TYPE | (3u<<1)),
  IDL_ULLONG = (IDL_LLONG | IDL_UNSIGNED),
  /* fixed size integer types overlap with legacy integer types in size, but
     unique identifiers are required for proper syntax errors. language
     bindings may choose to map onto different types as well */
  IDL_INT8 = (IDL_BASE_TYPE | IDL_INTEGER_TYPE | (4u<<1)),
  IDL_UINT8 = (IDL_INT8 | IDL_UNSIGNED),
  IDL_INT16 = (IDL_BASE_TYPE | IDL_INTEGER_TYPE | (5u<<1)),
  IDL_UINT16 = (IDL_INT16 | IDL_UNSIGNED),
  IDL_INT32 = (IDL_BASE_TYPE | IDL_INTEGER_TYPE | (6u<<1)),
  IDL_UINT32 = (IDL_INT32 | IDL_UNSIGNED),
  IDL_INT64 = (IDL_BASE_TYPE | IDL_INTEGER_TYPE | (7u<<1)),
  IDL_UINT64 = (IDL_INT64 | IDL_UNSIGNED),
  /* floating point types */
#define IDL_FLOATING_PT_TYPE (1llu<<8)
  IDL_FLOAT = (IDL_BASE_TYPE | IDL_FLOATING_PT_TYPE | 1u),
  IDL_DOUBLE = (IDL_BASE_TYPE | IDL_FLOATING_PT_TYPE | 2u),
  IDL_LDOUBLE = (IDL_BASE_TYPE | IDL_FLOATING_PT_TYPE | 3u)
};

#define IDL_TYPE_MASK ((IDL_TYPEDEF << 1) - 1)

typedef struct idl_name idl_name_t;
struct idl_name {
  idl_symbol_t symbol;
  char *identifier;
  bool is_annotation;
};

typedef struct idl_scoped_name idl_scoped_name_t;
struct idl_scoped_name {
  idl_symbol_t symbol;
  bool absolute;
  size_t length;
  idl_name_t **names;
  char *identifier; /**< qualified identifier */
};

typedef struct idl_field_name idl_field_name_t;
struct idl_field_name {
  idl_symbol_t symbol;
  size_t length;
  idl_name_t **names;
  char *identifier; /**< field identifier */
};

struct idl_scope;
struct idl_declaration;
struct idl_annotation_appl;

typedef void idl_const_expr_t;
typedef void idl_definition_t;
typedef void idl_type_spec_t;

typedef uint64_t idl_mask_t;
typedef void(*idl_delete_t)(void *node);
typedef void *(*idl_iterate_t)(const void *root, const void *node);
typedef const char *(*idl_describe_t)(const void *node);

typedef struct idl_node idl_node_t;
struct idl_node {
  idl_symbol_t symbol;
  idl_mask_t mask;
  idl_delete_t destructor;
  idl_iterate_t iterate;
  idl_describe_t describe;
  int32_t references;
  struct idl_annotation_appl *annotations;
  const struct idl_declaration *declaration;
  idl_node_t *parent;
  idl_node_t *previous, *next;
};

typedef struct idl_path idl_path_t;
struct idl_path {
  size_t length;
  const idl_node_t **nodes;
};

typedef enum idl_autoid idl_autoid_t;
enum idl_autoid {
  IDL_SEQUENTIAL,
  IDL_HASH
};

typedef enum idl_extensibility idl_extensibility_t;
enum idl_extensibility {
  IDL_FINAL,
  IDL_APPENDABLE,
  IDL_MUTABLE
};

typedef enum idl_try_construct idl_try_construct_t;
enum idl_try_construct {
  IDL_DISCARD,
  IDL_USE_DEFAULT,
  IDL_TRIM
};

typedef uint32_t allowable_data_representations_t;
#define IDL_ALLOWABLE_DATAREPRESENTATION_DEFAULT (0xffffffff)
typedef enum {
  IDL_DATAREPRESENTATION_FLAG_XCDR1 = 0x1 << 0,
  IDL_DATAREPRESENTATION_FLAG_XML   = 0x1 << 1,
  IDL_DATAREPRESENTATION_FLAG_XCDR2 = 0x1 << 2
} idl_data_representation_flags_t;

typedef enum {
  IDL_REQUIRES_XCDR2_UNSET,
  IDL_REQUIRES_XCDR2_SETTING,
  IDL_REQUIRES_XCDR2_TRUE,
  IDL_REQUIRES_XCDR2_FALSE
} idl_requires_xcdr2_t;

/* most types have convenience members for information that is shared between
   generators or makes sense to calculate in advance. e.g. the field
   identifier for struct members, which can be assigned through @id, @hashid,
   be assigned a value based on @autoid or the lack thereof. enumerator values
   are another good example. in both scenarios the value can be assigned
   through one or multiple annotations or automatically assigned when the
   enclosing scope is finalized. if the value is assigned through use of
   annotations, it must not be overwritten later on. alternatively, if
   conflicting annotations are used, it must be possible to throw an
   exception. therefore, each value that can be assigned explicitly, keeps a
   weak reference to the annotation, or is set to NULL. only annotations on
   the construct itself may be referenced. e.g. @default_nested annotations
   are only referenced by the annotated module, not by any subconstructs */
#define IDL_ANNOTATABLE(type) \
  struct { const struct idl_annotation_appl *annotation; type value; }

/* annotations */

typedef struct idl_const idl_const_t;
struct idl_const {
  idl_node_t node;
  idl_name_t *name;
  idl_type_spec_t *type_spec;
  idl_const_expr_t *const_expr;
};

/* constants contain the value of resolved constant expressions and are used
   if the resulting constant value can be of more than one type, e.g. in
   constant declarations, case labels, etc. language native types are used if
   the resulting constant value is required to be of a specific base type,
   e.g. bounds in sequences */
typedef struct idl_literal idl_literal_t;
struct idl_literal {
  idl_node_t node;
  union {
    bool bln;
    char chr;
    int8_t int8;
    uint8_t uint8;
    int16_t int16;
    uint16_t uint16;
    int32_t int32;
    uint32_t uint32;
    int64_t int64;
    uint64_t uint64;
    float flt;
    double dbl;
    long double ldbl;
    char *str;
  } value;
};

typedef struct idl_base_type idl_base_type_t;
struct idl_base_type {
  idl_node_t node;
  /* empty */
};

typedef struct idl_sequence idl_sequence_t;
struct idl_sequence {
  idl_node_t node;
  idl_type_spec_t *type_spec;
  uint32_t maximum;
};

typedef struct idl_string idl_string_t;
struct idl_string {
  idl_node_t node;
  uint32_t maximum;
};

typedef struct idl_module idl_module_t;
struct idl_module {
  idl_node_t node;
  idl_name_t *name;
  idl_definition_t *definitions;
  const idl_module_t *previous; /**< previous module if module was reopened */
  /* metadata */
  IDL_ANNOTATABLE(bool) default_nested;
  IDL_ANNOTATABLE(idl_autoid_t) autoid;
  IDL_ANNOTATABLE(allowable_data_representations_t) data_representation;
};

typedef struct idl_declarator idl_declarator_t;
struct idl_declarator {
  idl_node_t node;
  idl_name_t *name;
  idl_const_expr_t *const_expr;
  /* metadata */
  IDL_ANNOTATABLE(uint32_t) id;
};

typedef struct idl_member idl_member_t;
struct idl_member {
  idl_node_t node;
  idl_type_spec_t *type_spec;
  idl_declarator_t *declarators;
  /* metadata */
  IDL_ANNOTATABLE(bool) key;
  IDL_ANNOTATABLE(bool) optional;
  IDL_ANNOTATABLE(bool) external;
  IDL_ANNOTATABLE(bool) must_understand;
  IDL_ANNOTATABLE(idl_try_construct_t) try_construct;
  IDL_ANNOTATABLE(const idl_literal_t*) min;
  IDL_ANNOTATABLE(const idl_literal_t*) max;
  IDL_ANNOTATABLE(const idl_literal_t*) value;
  IDL_ANNOTATABLE(const char *) unit;
};

/* types can inherit from and extend other types (interfaces, values and
   structs). declarations in the base type that become available in the
   derived type as a consequence are imported into the scope */
typedef struct idl_inherit_spec idl_inherit_spec_t;
struct idl_inherit_spec {
  idl_node_t node;
  idl_type_spec_t *base;
};

/* keylist directives can use dotted names, e.g. "#pragma keylist foo bar.baz"
   in "struct foo { struct bar { long baz; }; };" to declare a member as key.
   this notation makes it possible for "baz" to only be a key member if "bar"
   is embedded in "foo" and for key order to differ from field order */
typedef struct idl_key idl_key_t;
struct idl_key {
  idl_node_t node;
  /* store entire field name as context matters for embedded struct fields */
  idl_field_name_t *field_name;
};

typedef struct idl_keylist idl_keylist_t;
struct idl_keylist {
  idl_node_t node;
  idl_key_t *keys;
};

typedef struct idl_struct idl_struct_t;
struct idl_struct {
  idl_node_t node;
  idl_inherit_spec_t *inherit_spec;
  struct idl_name *name;
  idl_member_t *members;
  /* metadata */
  idl_keylist_t *keylist; /**< if type is a topic (#pragma keylist) */
  idl_requires_xcdr2_t requires_xcdr2;
  IDL_ANNOTATABLE(uint32_t) id;
  IDL_ANNOTATABLE(idl_autoid_t) autoid;
  /* constructed types are not considered @nested types by default, implicitly
     stating the intent to use it as a topic. extensible and dynamic topic
     types added @default_nested and @topic to explicitly state the intent to
     use a type as a topic. for ease of use, the sum-total is provided as a
     single boolean */
  IDL_ANNOTATABLE(bool) nested;
  IDL_ANNOTATABLE(idl_extensibility_t) extensibility;
  IDL_ANNOTATABLE(allowable_data_representations_t) data_representation;
};

typedef struct idl_forward idl_forward_t;
struct idl_forward {
  idl_node_t node;
  struct idl_name *name;
  /* FIXME: no reference count is maintained for type specifier in forward
            declarations as it may introduce cyclic dependencies. perfect
            reason to remove use of destructors in Bison and use a pool
            allocator instead */
  idl_type_spec_t *type_spec;
};

typedef struct idl_case_label idl_case_label_t;
struct idl_case_label {
  idl_node_t node;
  void *const_expr;
};

typedef struct idl_case idl_case_t;
struct idl_case {
  idl_node_t node;
  idl_case_label_t *labels;
  idl_type_spec_t *type_spec;
  idl_declarator_t *declarator;
  IDL_ANNOTATABLE(bool) external;
  IDL_ANNOTATABLE(idl_try_construct_t) try_construct;
  IDL_ANNOTATABLE(const idl_literal_t*) min;
  IDL_ANNOTATABLE(const idl_literal_t*) max;
  IDL_ANNOTATABLE(const char *) unit;
};

typedef struct idl_switch_type_spec idl_switch_type_spec_t;
struct idl_switch_type_spec {
  idl_node_t node;
  idl_type_spec_t *type_spec;
  /* metadata */
  IDL_ANNOTATABLE(bool) key;
};

typedef struct idl_union idl_union_t;
struct idl_union {
  idl_node_t node;
  struct idl_name *name;
  idl_switch_type_spec_t *switch_type_spec;
  idl_case_t *cases;
  /* metadata */
  /* label associated with the default value for the discriminator. i.e.
     the first discriminant value if the entire range of the discriminator is
     covered, the default case if specified, or a spontaneously materialised
     implicit default case that does not reference any branch */
  idl_case_label_t *default_case;
  uint64_t unused_labels; /**< number of unused labels */
  idl_requires_xcdr2_t requires_xcdr2;
  IDL_ANNOTATABLE(bool) nested; /**< if type is nested or a topic */
  IDL_ANNOTATABLE(idl_extensibility_t) extensibility;
  IDL_ANNOTATABLE(idl_autoid_t) autoid;
  IDL_ANNOTATABLE(allowable_data_representations_t) data_representation;
};

typedef struct idl_enumerator idl_enumerator_t;
struct idl_enumerator {
  idl_node_t node;
  struct idl_name *name;
  /* metadata */
  /* an enumeration must contain no more than 2^32 enumerators and must be
     mapped to a native data type capable of representing a maximally-sized
     enumeration */
  IDL_ANNOTATABLE(uint32_t) value;
};

typedef struct idl_enum idl_enum_t;
struct idl_enum {
  idl_node_t node;
  struct idl_name *name;
  idl_enumerator_t *enumerators;
  idl_enumerator_t *default_enumerator;
  IDL_ANNOTATABLE(uint16_t) bit_bound;
  IDL_ANNOTATABLE(idl_extensibility_t) extensibility;
};

typedef struct idl_bit_value idl_bit_value_t;
struct idl_bit_value {
  idl_node_t node;
  struct idl_name *name;
  IDL_ANNOTATABLE(uint16_t) position;
};

typedef struct idl_bitmask idl_bitmask_t;
struct idl_bitmask {
  idl_node_t node;
  struct idl_name *name;
  idl_bit_value_t *bit_values;
  IDL_ANNOTATABLE(uint16_t) bit_bound;
  IDL_ANNOTATABLE(idl_extensibility_t) extensibility;
};

typedef struct idl_typedef idl_typedef_t;
struct idl_typedef {
  idl_node_t node;
  void *type_spec;
  idl_declarator_t *declarators;
};

struct idl_pstate;
typedef idl_retcode_t (*idl_annotation_callback_t)(
  struct idl_pstate *,
  struct idl_annotation_appl *,
  idl_node_t *);

typedef struct idl_annotation_member idl_annotation_member_t;
struct idl_annotation_member {
  idl_node_t node;
  idl_type_spec_t *type_spec;
  idl_declarator_t *declarator;
  idl_const_expr_t *const_expr; /**< default value (if any) */
};

typedef void idl_annotation_definition_t;

typedef struct idl_annotation idl_annotation_t;
struct idl_annotation {
  idl_node_t node;
  idl_name_t *name;
  /** definitions that together form the body, e.g. member, enum, etc */
  idl_definition_t *definitions;
  idl_annotation_callback_t callback;
};

typedef struct idl_annotation_appl_param idl_annotation_appl_param_t;
struct idl_annotation_appl_param {
  idl_node_t node;
  idl_annotation_member_t *member;
  idl_const_expr_t *const_expr; /**< constant or enumerator */
};

typedef struct idl_annotation_appl idl_annotation_appl_t;
struct idl_annotation_appl {
  idl_node_t node;
  const idl_annotation_t *annotation;
  idl_annotation_appl_param_t *parameters;
};

typedef enum idl_keytype {
  IDL_KEYTYPE_NONE,
  IDL_KEYTYPE_IMPLICIT,
  IDL_KEYTYPE_EXPLICIT
} idl_keytype_t;

IDL_EXPORT bool idl_is_declaration(const void *node);
IDL_EXPORT bool idl_is_module(const void *node);
IDL_EXPORT bool idl_is_const(const void *node);
IDL_EXPORT bool idl_is_literal(const void *node);
IDL_EXPORT bool idl_is_type_spec(const void *node);
IDL_EXPORT bool idl_is_base_type(const void *node);
IDL_EXPORT bool idl_is_floating_pt_type(const void *node);
IDL_EXPORT bool idl_is_integer_type(const void *node);
IDL_EXPORT bool idl_is_templ_type(const void *node);
IDL_EXPORT bool idl_is_bounded(const void *node);
IDL_EXPORT bool idl_is_sequence(const void *node);
IDL_EXPORT bool idl_is_string(const void *node);
IDL_EXPORT bool idl_is_unbounded_string(const void *node);
IDL_EXPORT bool idl_is_bounded_string(const void *node);
IDL_EXPORT bool idl_is_constr_type(const void *node);
IDL_EXPORT bool idl_is_struct(const void *node);
IDL_EXPORT bool idl_is_empty(const void *node);
IDL_EXPORT bool idl_is_inherit_spec(const void *node);
IDL_EXPORT bool idl_is_member(const void *node);
IDL_EXPORT bool idl_is_union(const void *node);
IDL_EXPORT bool idl_is_switch_type_spec(const void *node);
IDL_EXPORT bool idl_is_case(const void *node);
IDL_EXPORT bool idl_is_default_case(const void *node);
IDL_EXPORT bool idl_is_implicit_default_case(const void *ptr);
IDL_EXPORT bool idl_is_case_label(const void *node);
IDL_EXPORT bool idl_is_default_case_label(const void *node);
IDL_EXPORT bool idl_is_implicit_default_case_label(const void *node);
IDL_EXPORT bool idl_is_enum(const void *node);
IDL_EXPORT bool idl_is_enumerator(const void *node);
IDL_EXPORT bool idl_is_bitmask(const void *node);
IDL_EXPORT bool idl_is_bit_value(const void *node);
IDL_EXPORT bool idl_is_alias(const void *node);
IDL_EXPORT bool idl_is_typedef(const void *node);
IDL_EXPORT bool idl_is_declarator(const void *node);
IDL_EXPORT bool idl_is_array(const void *node);
IDL_EXPORT bool idl_is_annotation_member(const void *node);
IDL_EXPORT bool idl_is_annotation_appl(const void *node);
IDL_EXPORT bool idl_is_topic(const void *node, bool keylist);
IDL_EXPORT bool idl_is_keyless(const void *node, bool keylist);
IDL_EXPORT bool idl_is_forward(const void *node);
IDL_EXPORT idl_keytype_t idl_is_topic_key(const void *node, bool keylist, const idl_path_t *path, uint32_t *order);
IDL_EXPORT bool idl_is_extensible(const idl_node_t *node, idl_extensibility_t extensibility);
IDL_EXPORT bool idl_has_unset_extensibility_r(idl_node_t *node);
IDL_EXPORT bool idl_is_external(const idl_node_t *node);
IDL_EXPORT bool idl_is_optional(const idl_node_t *node);
IDL_EXPORT bool idl_is_must_understand(const idl_node_t *node);
IDL_EXPORT int64_t idl_case_label_intvalue(const void *ptr);

/* accessors */
IDL_EXPORT idl_mask_t idl_mask(const void *node);
IDL_EXPORT idl_type_t idl_type(const void *node);
IDL_EXPORT idl_type_spec_t *idl_type_spec(const void *node);
/* return a string describing the language construct. e.g. "module" or
   "forward struct" for modules and forward struct declarations respectively */
IDL_EXPORT const char *idl_construct(const void *node);
IDL_EXPORT const char *idl_identifier(const void *node);
IDL_EXPORT bool idl_identifier_is(const void *node, const char *identifier);
IDL_EXPORT const idl_name_t *idl_name(const void *node);
IDL_EXPORT uint32_t idl_array_size(const void *node);
IDL_EXPORT uint32_t idl_bound(const void *node);
IDL_EXPORT const idl_literal_t *idl_default_value(const void *node);
IDL_EXPORT bool idl_requires_xcdr2(const void *node);
IDL_EXPORT allowable_data_representations_t idl_allowable_data_representations(const void *node);
IDL_EXPORT uint32_t idl_enum_max_value(const void *node);

/* navigation */
IDL_EXPORT void *idl_ancestor(const void *node, size_t levels);
IDL_EXPORT void *idl_parent(const void *node);
IDL_EXPORT size_t idl_degree(const void *node);
IDL_EXPORT void *idl_next(const void *node);
IDL_EXPORT void *idl_previous(const void *node);
IDL_EXPORT void *idl_iterate(const void *root, const void *node);

#define IDL_FOREACH(node, list) \
  for ((node) = (list); (node); (node) = idl_next(node))

IDL_EXPORT void *idl_unalias(const void *node);

#define IDL_STRIP_ALIASES (1u<<0) /**< expose base type of aliase(s) for a non-array type */
#define IDL_STRIP_ALIASES_ARRAY (1u<<1) /**< expose base type of aliase(s) for array type */
#define IDL_STRIP_FORWARD (1u<<2) /**< expose definition */
IDL_EXPORT void *idl_strip(const void *node, uint32_t flags);

#endif /* IDL_TREE_H */
