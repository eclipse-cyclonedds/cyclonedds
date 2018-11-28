/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDS_TS_TYPETREE_H
#define DDS_TS_TYPETREE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * The bits used for the flags are:
 * 0-7: the specific base type
 * 8-13: the type spec
 * 14-15: the definitions
 * 16-: the other nodes
 */

#define DDS_TS_TYPE_SPEC(X)             (((X)&7)<<8)
#define DDS_TS_IS_TYPE_SPEC(T)          (((7<<8)&(T)) != 0)
#define DDS_TS_BASE_TYPE(X)             (DDS_TS_TYPE_SPEC(1) | (X))
#define DDS_TS_IS_BASE_TYPE(T)          (((7<<8)&(T)) == 1)
#define DDS_TS_GET_BASE_TYPE(T)         (((7<<8)|31)&(T))
#define DDS_TS_SHORT_TYPE               DDS_TS_BASE_TYPE(1)
#define DDS_TS_LONG_TYPE                DDS_TS_BASE_TYPE(2)
#define DDS_TS_LONG_LONG_TYPE           DDS_TS_BASE_TYPE(3)
#define DDS_TS_UNSIGNED_SHORT_TYPE      DDS_TS_BASE_TYPE(4)
#define DDS_TS_UNSIGNED_LONG_TYPE       DDS_TS_BASE_TYPE(5)
#define DDS_TS_UNSIGNED_LONG_LONG_TYPE  DDS_TS_BASE_TYPE(6)
#define DDS_TS_CHAR_TYPE                DDS_TS_BASE_TYPE(7)
#define DDS_TS_WIDE_CHAR_TYPE           DDS_TS_BASE_TYPE(8)
#define DDS_TS_BOOLEAN_TYPE             DDS_TS_BASE_TYPE(9)
#define DDS_TS_OCTET_TYPE               DDS_TS_BASE_TYPE(10)
#define DDS_TS_INT8_TYPE                DDS_TS_BASE_TYPE(11)
#define DDS_TS_UINT8_TYPE               DDS_TS_BASE_TYPE(12)
#define DDS_TS_FLOAT_TYPE               DDS_TS_BASE_TYPE(13)
#define DDS_TS_DOUBLE_TYPE              DDS_TS_BASE_TYPE(14)
#define DDS_TS_LONG_DOUBLE_TYPE         DDS_TS_BASE_TYPE(15)
#define DDS_TS_FIXED_PT_CONST_TYPE      DDS_TS_BASE_TYPE(16)
#define DDS_TS_ANY_TYPE                 DDS_TS_BASE_TYPE(17)
#define DDS_TS_SEQUENCE                 DDS_TS_TYPE_SPEC(2)
#define DDS_TS_STRING                   DDS_TS_TYPE_SPEC(3)
#define DDS_TS_WIDE_STRING              DDS_TS_TYPE_SPEC(4)
#define DDS_TS_FIXED_PT                 DDS_TS_TYPE_SPEC(5)
#define DDS_TS_MAP                      DDS_TS_TYPE_SPEC(6)
#define DDS_TS_DEFINITION(X)            (((X)&15)<<12)
#define DDS_TS_IS_DEFINITION(T)         (((15<<12)&(T)) != 0)
#define DDS_TS_MODULE                   DDS_TS_DEFINITION(1)
#define DDS_TS_FORWARD_STRUCT           DDS_TS_DEFINITION(2)
#define DDS_TS_STRUCT                   DDS_TS_DEFINITION(3)
#define DDS_TS_DECLARATOR               DDS_TS_DEFINITION(4)
#define DDS_TS_OTHER(X)                 ((X)<<16)
#define DDS_TS_STRUCT_MEMBER            DDS_TS_OTHER(1)
#define DDS_TS_ARRAY_SIZE               DDS_TS_OTHER(2)

/* Open issues:
 * - include file and line information in type definitions.
 */

typedef char *dds_ts_identifier_t;

/* Literals
 *
 * Literals are values, either stated or calculated with an expression, that
 * appear in the IDL declaration. The literals only appear as members of
 * IDL elements, such as the constant definition and the case labels.
 */

typedef uint32_t dds_ts_node_flags_t;

typedef struct {
  dds_ts_node_flags_t flags; /* flags defining the kind of the literal */
  union {
    bool bln;
    char chr;
    unsigned long wchr;
    char *str;
    unsigned long long ullng;
    signed long long llng;
    long double ldbl;
  } value;
} dds_ts_literal_t;


/* Generic node
 *
 * The generic node serves as a basis for all other elements of the IDL type
 * definitions
 */

typedef struct dds_ts_node dds_ts_node_t;
struct dds_ts_node {
  dds_ts_node_flags_t flags; /* flags defining the kind of the node */
  dds_ts_node_t *parent;     /* pointer to the parent node */
  dds_ts_node_t *children;   /* pointer to the first child */
  dds_ts_node_t *next;       /* pointer to the next sibling */
  /* Maybe also needs information about the file and line where the node has
   * been parsed from. This is maybe needed to determine for which parts
   * of the preprocessed output code needs to be generated.
   */
  void (*free_func)(dds_ts_node_t*);
};

void dds_ts_free_node(dds_ts_node_t *node);


/* Type specifications */

/* Type specification (type_spec) */
typedef struct {
  dds_ts_node_t node;
} dds_ts_type_spec_t;

/* Base type specification (base_type_spec) */
typedef struct {
  dds_ts_type_spec_t type_spec;
} dds_ts_base_type_t;

dds_ts_base_type_t *dds_ts_create_base_type(dds_ts_node_flags_t flags);

/* Pointer to type spec */
typedef struct {
  dds_ts_type_spec_t *type_spec;
  bool is_reference;
} dds_ts_type_spec_ptr_t;

void dds_ts_type_spec_ptr_assign(dds_ts_type_spec_ptr_t *type_spec_ptr, dds_ts_type_spec_t *type_spec);
void dds_ts_type_spec_ptr_assign_reference(dds_ts_type_spec_ptr_t *type_spec_ptr, dds_ts_type_spec_t *type_spec);

/* Sequence type (sequence_type) */
typedef struct {
  dds_ts_type_spec_t type_spec;
  dds_ts_type_spec_ptr_t element_type;
  bool bounded;
  unsigned long long max;
} dds_ts_sequence_t;

dds_ts_sequence_t *dds_ts_create_sequence(dds_ts_type_spec_ptr_t* element_type, bool bounded, unsigned long long max);

/* (Wide) string type (string_type, wide_string_type) */
typedef struct {
  dds_ts_type_spec_t type_spec;
  bool bounded;
  unsigned long long max;
} dds_ts_string_t;

dds_ts_string_t *dds_ts_create_string(dds_ts_node_flags_t flags, bool bounded, unsigned long long max);

/* Fixed point type (fixed_pt_type) */
typedef struct {
  dds_ts_type_spec_t type_spec;
  unsigned long long digits;
  unsigned long long fraction_digits;
} dds_ts_fixed_pt_t;

dds_ts_fixed_pt_t *dds_ts_create_fixed_pt(unsigned long long digits, unsigned long long fraction_digits);

/* Map type (map_type) */
typedef struct {
  dds_ts_type_spec_t type_spec;
  dds_ts_type_spec_ptr_t key_type;
  dds_ts_type_spec_ptr_t value_type;
  bool bounded;
  unsigned long long max;
} dds_ts_map_t;

dds_ts_map_t *dds_ts_create_map(dds_ts_type_spec_ptr_t *key_type, dds_ts_type_spec_ptr_t *value_type, bool bounded, unsigned long long max);


/* Type definitions */

/* Definition (definition)
 * (all definitions share a name)
 */
typedef struct {
  dds_ts_type_spec_t type_spec;
  dds_ts_identifier_t name;
} dds_ts_definition_t;

/* Module declaration (module_dcl)
 * - defintions appear as children
 * (Probably needs extra member (and type) for definitions that are introduced
 * by non-top scoped names when checking the scope rules.)
 */
typedef struct {
  dds_ts_definition_t def;
} dds_ts_module_t;

dds_ts_module_t *dds_ts_create_module(dds_ts_identifier_t name);

/* Forward declaration */
typedef struct {
  dds_ts_definition_t def;
  dds_ts_definition_t *definition; /* reference to the actual definition */
} dds_ts_forward_declaration_t;

/* Struct forward declaration (struct_forward_dcl)
 * dds_ts_forward_declaration_t (no extra members)
 */

dds_ts_forward_declaration_t *dds_ts_create_struct_forward_dcl(dds_ts_identifier_t name);

/* Struct declaration (struct_def)
 * - members appear as children
 */
typedef struct {
  dds_ts_definition_t def;
  dds_ts_definition_t *super; /* used for extended struct type definition */
} dds_ts_struct_t;

dds_ts_struct_t *dds_ts_create_struct(dds_ts_identifier_t name);

/* Struct member (members)
 * - declarators appear as chidren
 */
typedef struct {
  dds_ts_node_t node;
  dds_ts_type_spec_ptr_t member_type;
} dds_ts_struct_member_t;

dds_ts_struct_member_t *dds_ts_create_struct_member(dds_ts_type_spec_ptr_t *member_type);

/* Declarator (declarator)
 * - fixed array sizes appear as children
 * use dds_ts_definition_t (no extra members)
 */

dds_ts_definition_t *dds_ts_create_declarator(dds_ts_identifier_t name);

/* Fixed array size (fixed_array_size) */
typedef struct {
  dds_ts_node_t node;
  unsigned long long size;
} dds_ts_array_size_t;

dds_ts_array_size_t *dds_ts_create_array_size(unsigned long long size);


#endif /* DDS_TYPE_TYPETREE_H */
