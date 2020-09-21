/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef IDL_TREE_H
#define IDL_TREE_H

/**
 * @file
 * Types and functions for IDL compiler backends.
 */

#include <stdbool.h>
#include <stdint.h>

#include "idl/export.h"
#include "idl/retcode.h"

struct idl_scope;
struct idl_name;
struct idl_scoped_name;

typedef struct idl_file idl_file_t;
struct idl_file {
  idl_file_t *next;
  char *name;
};

typedef struct idl_position idl_position_t;
struct idl_position {
  const char *file;
  uint32_t line;
  uint32_t column;
};

typedef struct idl_location idl_location_t;
struct idl_location {
  idl_position_t first;
  idl_position_t last;
};

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

/** Member is (part of) the key */
#define IDL_KEY (1llu<<37)                        /* IDL_PRAGMA / IDL_MEMBER */
/** Identifier has been assigned (and set) using either @id or, if DDS-XTypes
    has been enabled, using @hashid. */
#define IDL_ID (1llu<<36)                                      /* IDL_MEMBER */
/* directives */
#define IDL_DIRECTIVE (1llu<<35)
#define IDL_LINE (1llu<<34)
#define IDL_PRAGMA (1llu<<33)
#define   IDL_KEYLIST (IDL_PRAGMA | 1llu)
#define   IDL_DATA_TYPE (IDL_PRAGMA | 2llu)
/* expressions */
#define IDL_EXPR (1llu<<32)
#define IDL_BINARY_EXPR (1llu<<31)                              /* IDL_EXPR */
#define   IDL_OR_EXPR (IDL_BINARY_EXPR | 1llu)
#define   IDL_XOR_EXPR (IDL_BINARY_EXPR | 2llu)
#define   IDL_AND_EXPR (IDL_BINARY_EXPR | 3llu)
#define   IDL_LSHIFT_EXPR (IDL_BINARY_EXPR | 4llu)
#define   IDL_RSHIFT_EXPR (IDL_BINARY_EXPR | 5llu)
#define   IDL_ADD_EXPR (IDL_BINARY_EXPR | 6llu)
#define   IDL_SUB_EXPR (IDL_BINARY_EXPR | 7llu)
#define   IDL_MULT_EXPR (IDL_BINARY_EXPR | 8llu)
#define   IDL_DIV_EXPR (IDL_BINARY_EXPR | 9llu)
#define   IDL_MOD_EXPR (IDL_BINARY_EXPR | 10llu)
#define IDL_UNARY_EXPR (1llu<<30)                               /* IDL_EXPR */
#define   IDL_MINUS_EXPR (IDL_UNARY_EXPR | 1llu)
#define   IDL_PLUS_EXPR (IDL_UNARY_EXPR | 2llu)
#define   IDL_NOT_EXPR (IDL_UNARY_EXPR | 3llu)
#define IDL_LITERAL (1llu<<29)                                  /* IDL_EXPR */
#define   IDL_INTEGER_LITERAL (IDL_LITERAL | IDL_ULLONG)
#define   IDL_FLOATING_PT_LITERAL (IDL_LITERAL | IDL_LDOUBLE)
#define   IDL_CHAR_LITERAL (IDL_LITERAL | IDL_CHAR)
#define   IDL_WCHAR_LITERAL (IDL_LITERAL | IDL_WCHAR)
#define   IDL_BOOLEAN_LITERAL (IDL_LITERAL | IDL_BOOL)
#define   IDL_STRING_LITERAL (IDL_LITERAL | IDL_STRING)
#define   IDL_WSTRING_LITERAL (IDL_LITERAL | IDL_WSTRING)
/* specifiers */
#define IDL_TYPE (1llu<<28)
/* declarations */
#define IDL_DECL (1llu<<27)
#define IDL_MODULE (1llu<<26)                                   /* IDL_DECL */
#define IDL_CONST (1llu<<25)                           /* IDL_DECL / <type> */
#define IDL_TYPEDEF (1llu<<24)                                  /* IDL_DECL */
#define IDL_MEMBER (1llu<<23)                                   /* IDL_DECL */
#define IDL_FORWARD (1llu<<22)                    /* IDL_STRUCT / IDL_UNION */
#define IDL_CASE (1llu<<21)                                     /* IDL_DECL */
#define IDL_CASE_LABEL (1llu<<20)                               /* IDL_DECL */
#define IDL_ENUMERATOR (1llu<<19)                               /* IDL_DECL */
#define IDL_DECLARATOR (1llu<<18)                               /* IDL_DECL */
#define IDL_ANNOTATION_APPL (1llu<<17)                          /* IDL_DECL */
#define IDL_ANNOTATION_APPL_PARAM (1llu<<16)                    /* IDL_DECL */
/* constructed types */
#define IDL_CONSTR_TYPE (1llu<<15)                   /* IDL_TYPE | IDL_DECL */
#define IDL_STRUCT (1llu<<14)                            /* IDL_CONSTR_TYPE */
#define IDL_UNION (1llu<<13)                             /* IDL_CONSTR_TYPE */
#define IDL_ENUM (1llu<<12)                              /* IDL_CONSTR_TYPE */
/* template types */
#define IDL_TEMPL_TYPE (1llu<<11)
#define   IDL_SEQUENCE (IDL_TEMPL_TYPE | 1llu)                  /* IDL_TYPE */
#define   IDL_STRING (IDL_TEMPL_TYPE | 2llu)        /* IDL_CONST / IDL_TYPE */
#define   IDL_WSTRING (IDL_TEMPL_TYPE | 3llu)       /* IDL_CONST / IDL_TYPE */
#define   IDL_FIXED_PT (IDL_TEMPL_TYPE | 4llu)                  /* IDL_TYPE */
/* simple types */
/* miscellaneous base types */
#define IDL_BASE_TYPE (1llu<<10)                    /* IDL_CONST / IDL_TYPE */
#define   IDL_CHAR (IDL_BASE_TYPE | 1llu)
#define   IDL_WCHAR (IDL_BASE_TYPE | 2llu)
#define   IDL_BOOL (IDL_BASE_TYPE | 3llu)
#define   IDL_OCTET (IDL_BASE_TYPE | 4llu)
/* integer types */
#define IDL_INTEGER_TYPE (IDL_BASE_TYPE | 1llu<<9)         /* IDL_BASE_TYPE */
#define IDL_UNSIGNED (1llu<<0)
#define   IDL_INT8 (IDL_INTEGER_TYPE | (1llu<<1))
#define   IDL_UINT8 (IDL_INT8 | IDL_UNSIGNED)
#define   IDL_INT16 (IDL_INTEGER_TYPE | (2llu<<1))
#define   IDL_UINT16 (IDL_INT16 | IDL_UNSIGNED)
#define   IDL_SHORT (IDL_INT16)
#define   IDL_USHORT (IDL_UINT16)
#define   IDL_INT32 (IDL_INTEGER_TYPE | (3llu<<1))
#define   IDL_UINT32 (IDL_INT32 | IDL_UNSIGNED)
#define   IDL_LONG (IDL_INT32)
#define   IDL_ULONG (IDL_UINT32)
#define   IDL_INT64 (IDL_INTEGER_TYPE | (4llu<<1))
#define   IDL_UINT64 (IDL_INT64 | IDL_UNSIGNED)
#define   IDL_LLONG (IDL_INT64)
#define   IDL_ULLONG (IDL_UINT64)
/* floating point types */
#define IDL_FLOATING_PT_TYPE (IDL_BASE_TYPE | (1llu<<8))   /* IDL_BASE_TYPE */
#define   IDL_FLOAT (IDL_FLOATING_PT_TYPE | 1llu)
#define   IDL_DOUBLE (IDL_FLOATING_PT_TYPE | 2llu)
#define   IDL_LDOUBLE (IDL_FLOATING_PT_TYPE | 3llu)

typedef void(*idl_print_t)(const void *node);
typedef void(*idl_delete_t)(void *node);

typedef uint64_t idl_mask_t;

typedef struct idl_annotation_appl idl_annotation_appl_t;
typedef struct idl_node idl_node_t;
struct idl_node {
  idl_mask_t mask; /**< node type, e.g. integer literal, struct, etc */
  int16_t deleted; /**< whether or not node was deleted */
  int16_t references; /**< number of references to node */
  idl_location_t location;
  idl_annotation_appl_t *annotations;
  const struct idl_scope *scope;
  idl_node_t *parent; /**< pointer to parent node */
  idl_node_t *previous, *next; /**< pointers to sibling nodes */
  idl_print_t printer;
  idl_delete_t destructor;
};

typedef struct idl_tree idl_tree_t;
struct idl_tree {
  idl_node_t *root;
  idl_file_t *files;
};

/* syntactic sugar */
typedef idl_node_t idl_definition_t;
typedef idl_node_t idl_type_spec_t;
typedef idl_node_t idl_simple_type_spec_t;
typedef idl_node_t idl_constr_type_spec_t;
typedef idl_node_t idl_switch_type_spec_t;
typedef idl_node_t idl_const_type_t;
typedef idl_node_t idl_const_expr_t;
typedef idl_node_t idl_primary_expr_t;

typedef struct idl_binary_expr idl_binary_expr_t;
struct idl_binary_expr {
  idl_node_t node;
  idl_const_expr_t *left;
  idl_const_expr_t *right;
};

typedef struct idl_unary_expr idl_unary_expr_t;
struct idl_unary_expr {
  idl_node_t node;
  idl_const_expr_t *right;
};

typedef struct idl_literal idl_literal_t;
struct idl_literal {
  idl_node_t node;
  union {
    bool bln;
    uint64_t ullng;
    double dbl;
    long double ldbl;
    char *str;
  } value;
};

/* constants contain the value of resolved constant expressions and are used
   if the resulting constant value can be of more than one type, e.g. in
   constant declarations, case labels, etc. language native types are used if
   the resulting constant value is required to be of a specific base type,
   e.g. bounds in sequences. */
typedef struct idl_constval idl_constval_t;
struct idl_constval {
  idl_node_t node;
  union {
    bool bln;
    char chr;
    int8_t int8;
    uint8_t uint8, oct;
    int16_t int16, shrt;
    uint16_t uint16, ushrt;
    int32_t int32, lng;
    uint32_t uint32, ulng;
    int64_t int64, llng;
    uint64_t uint64, ullng;
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
  idl_simple_type_spec_t *type_spec;
  uint64_t maximum; /**< maximum size of bounded sequence or 0 if unbounded */
};

typedef struct idl_string idl_string_t;
struct idl_string {
  idl_node_t node;
  uint64_t maximum; /**< maximum size of bounded string or 0 if unbounded */
};

/* annotations */
typedef struct idl_annotation_appl_param idl_annotation_appl_param_t;
struct idl_annotation_appl_param {
  idl_node_t node;
  struct idl_name *name;
  idl_const_expr_t *const_expr;
};

struct idl_annotation_appl {
  idl_node_t node;
  struct idl_scoped_name *scoped_name;
  /* FIXME: either an expression or a list of parameters, needs work */
  idl_annotation_appl_param_t *parameters;
};

typedef struct idl_const idl_const_t;
struct idl_const {
  idl_node_t node;
  idl_const_type_t *type_spec;
  struct idl_name *name;
  idl_const_expr_t *const_expr;
};

/* definitions */
typedef struct idl_module idl_module_t;
struct idl_module {
  idl_node_t node;
  struct idl_name *name;
  idl_definition_t *definitions;
};

typedef struct idl_declarator idl_declarator_t;
struct idl_declarator {
  idl_node_t node;
  struct idl_name *name;
  idl_const_expr_t *const_expr; /**< array sizes */
};

/* #pragma keylist directives and @key annotations can be mixed if the
   key members and ordering match. both are converted to populate the key
   member in the second pass */

typedef struct idl_member idl_member_t;
struct idl_member {
  idl_node_t node;
  idl_type_spec_t *type_spec;
  idl_declarator_t *declarators;
  uint32_t id; /**< member id, paired with IDL_ID */
  uint32_t key; /**< key order for IDL 3.5, paired with IDL_KEY */
};

/**
 * Complements @id annotation and is applicable to any set containing elements
 * to which allocating a 32-bit integer identifier makes sense. It instructs
 * to automatically allocate identifiers to the elements.
 */
typedef enum idl_autoid idl_autoid_t;
enum idl_autoid {
  /** Identifier computed by incrementing previous. */
  IDL_AUTOID_SEQUENTIAL,
  /** Identifier computed by a hashing algorithm. */
  IDL_AUTOID_HASH
};

typedef enum idl_extensibility idl_extensibility_t;
enum idl_extensibility {
  /** Type is not allowed to evolve */
  IDL_EXTENSIBILITY_FINAL,
  /** Type may be complemented (elements may be appended, not reorganized) */
  IDL_EXTENSIBILITY_APPENDABLE,
  /** Type may evolve */
  IDL_EXTENSIBILITY_MUTABLE
};

typedef struct idl_struct idl_struct_t;
struct idl_struct {
  idl_node_t node;
  idl_struct_t *base_type; /**< Base type extended by struct (optional) */
  struct idl_name *name;
  idl_member_t *members;
  idl_autoid_t autoid;
  idl_extensibility_t extensibility;
};

typedef struct idl_case_label idl_case_label_t;
struct idl_case_label {
  idl_node_t node;
  idl_const_expr_t *const_expr; /**< variant or enumerator */
};

typedef struct idl_case idl_case_t;
struct idl_case {
  idl_node_t node;
  idl_case_label_t *case_labels;
  idl_type_spec_t *type_spec;
  idl_declarator_t *declarator;
};

typedef struct idl_union idl_union_t;
struct idl_union {
  idl_node_t node;
  struct idl_name *name;
  idl_switch_type_spec_t *switch_type_spec;
  idl_case_t *cases;
};

typedef struct idl_enumerator idl_enumerator_t;
struct idl_enumerator {
  idl_node_t node;
  struct idl_name *name;
  uint32_t value;
};

typedef struct idl_enum idl_enum_t;
struct idl_enum {
  idl_node_t node;
  struct idl_name *name;
  idl_enumerator_t *enumerators;
};

typedef struct idl_forward idl_forward_t;
struct idl_forward {
  idl_node_t node;
  struct idl_name *name;
  idl_constr_type_spec_t *constr_type; /**< union or struct declaration */
};

typedef struct idl_typedef idl_typedef_t;
struct idl_typedef {
  idl_node_t node;
  idl_type_spec_t *type_spec;
  idl_declarator_t *declarators;
};

IDL_EXPORT bool idl_is_declaration(const void *node);
IDL_EXPORT bool idl_is_module(const void *node);
IDL_EXPORT bool idl_is_struct(const void *node);
IDL_EXPORT bool idl_is_member(const void *node);
IDL_EXPORT bool idl_is_union(const void *node);
IDL_EXPORT bool idl_is_case(const void *node);
IDL_EXPORT bool idl_is_default_case(const void *node);
IDL_EXPORT bool idl_is_case_label(const void *node);
IDL_EXPORT bool idl_is_enum(const void *node);
IDL_EXPORT bool idl_is_declarator(const void *node);
IDL_EXPORT bool idl_is_enumerator(const void *node);
IDL_EXPORT bool idl_is_type_spec(const void *node, idl_mask_t mask);
IDL_EXPORT bool idl_is_typedef(const void *node);
IDL_EXPORT bool idl_is_forward(const void *node);
IDL_EXPORT bool idl_is_templ_type(const void *node);
IDL_EXPORT bool idl_is_sequence(const void *node);
IDL_EXPORT bool idl_is_string(const void *node);
IDL_EXPORT bool idl_is_base_type(const void *node);

IDL_EXPORT bool idl_is_masked(const void *node, idl_mask_t mask);
IDL_EXPORT const char *idl_identifier(const void *node);
IDL_EXPORT const idl_location_t *idl_location(const void *node);
IDL_EXPORT void *idl_parent(const void *node);
IDL_EXPORT void *idl_previous(const void *node);
IDL_EXPORT void *idl_next(const void *node);
IDL_EXPORT void *idl_unalias(const void *node);

IDL_EXPORT idl_retcode_t
idl_parse_string(const char *str, uint32_t flags, idl_tree_t **treeptr);

IDL_EXPORT void
idl_delete_tree(idl_tree_t *tree);

#endif /* IDL_TREE_H */
