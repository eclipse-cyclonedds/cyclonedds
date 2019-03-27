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
#ifndef DDSTS_TYPETREE_H
#define DDSTS_TYPETREE_H

#include <stdbool.h>
#include <stdint.h>
#include "dds/ddsrt/retcode.h"
#include "dds/export.h"

#define DDSTS_TYPE(X)                  (1L<<(X))

#define DDSTS_BASIC_TYPES              ((1L<<(16+1)-1L)
#define DDSTS_SHORT                    DDSTS_TYPE(0)
#define DDSTS_LONG                     DDSTS_TYPE(1)
#define DDSTS_LONGLONG                 DDSTS_TYPE(2)
#define DDSTS_USHORT                   DDSTS_TYPE(3)
#define DDSTS_ULONG                    DDSTS_TYPE(4)
#define DDSTS_ULONGLONG                DDSTS_TYPE(5)
#define DDSTS_CHAR                     DDSTS_TYPE(6)
#define DDSTS_WIDE_CHAR                DDSTS_TYPE(7)
#define DDSTS_BOOLEAN                  DDSTS_TYPE(8)
#define DDSTS_OCTET                    DDSTS_TYPE(9)
#define DDSTS_INT8                     DDSTS_TYPE(10)
#define DDSTS_UINT8                    DDSTS_TYPE(11)
#define DDSTS_FLOAT                    DDSTS_TYPE(12)
#define DDSTS_DOUBLE                   DDSTS_TYPE(13)
#define DDSTS_LONGDOUBLE               DDSTS_TYPE(14)
#define DDSTS_FIXED_PT_CONST           DDSTS_TYPE(15)
#define DDSTS_ANY                      DDSTS_TYPE(16)

#define DDSTS_SEQUENCE                 DDSTS_TYPE(17)
#define DDSTS_ARRAY                    DDSTS_TYPE(18)
#define DDSTS_STRING                   DDSTS_TYPE(19)
#define DDSTS_WIDE_STRING              DDSTS_TYPE(20)
#define DDSTS_FIXED_PT                 DDSTS_TYPE(21)
#define DDSTS_MAP                      DDSTS_TYPE(22)

#define DDSTS_DEFINITIONS              (((1L<<(26-23+1))-1L)<<23)
#define DDSTS_MODULE                   DDSTS_TYPE(23)
#define DDSTS_FORWARD_STRUCT           DDSTS_TYPE(24)
#define DDSTS_STRUCT                   DDSTS_TYPE(25)
#define DDSTS_DECLARATION              DDSTS_TYPE(26)

#define DDSTS_TYPES                    ((1L<<(26+1))-1L)
#define DDSTS_TYPE_OF(O)               ((O)->type.flags & DDSTS_TYPES)
#define DDSTS_IS_TYPE(O,T)             (DDSTS_TYPE_OF(O) == (T))
#define DDSTS_IS_DEFINITION(O)         ((DDSTS_DEFINITIONS & (O)->type.flags) != 0)

#define DDSTS_UNBOUND                  (1L<<27)
#define DDSTS_IS_UNBOUND(O)            (((O)->type.flags & DDSTS_UNBOUND) != 0)

#define DDSTS_REFERENCE_1              (1L<<28)
#define DDSTS_REFERENCE_2              (1L<<29)

typedef char *ddsts_identifier_t;

/**
 * Literals
 *
 * Literals are values, either stated or calculated with an expression, that
 * appear in the IDL declaration. The literals only appear as members of
 * IDL elements, such as the constant definition and the case labels.
 */

typedef uint32_t ddsts_flags_t;

typedef struct {
  ddsts_flags_t flags; /* flags defining the kind of the literal */
  union {
    bool bln;
    char chr;
    unsigned long wchr;
    char *str;
    unsigned long long ullng;
    signed long long llng;
    long double ldbl;
  } value;
} ddsts_literal_t;

DDS_EXPORT void ddsts_free_literal(ddsts_literal_t *literal);


/**
 * Type specification
 *
 * The union ddsts_type_t is used to contain all possible type specifications,
 * where the struct ddsts_typespec_t serves as a basis for these type specifications.
 */

typedef union ddsts_type ddsts_type_t;

DDS_EXPORT void ddsts_free_type(ddsts_type_t *type);

typedef struct ddsts_typespec ddsts_typespec_t;
struct ddsts_typespec {
  ddsts_flags_t flags;      /* flags defining the kind of the type */
  ddsts_identifier_t name;
  ddsts_type_t *parent;     /* pointer to the parent type, implying ownership */
  ddsts_type_t *next;       /* pointer to the next sibling */
  void (*free_func)(ddsts_type_t*);
};

/* Base type specification (base_type_spec) */
typedef struct {
  ddsts_typespec_t typespec;
} ddsts_base_type_t;

/**
 * @brief Creates a ddsts_base_type_t struct and sets it with flags
 *
 * @param[in]   flags   One of DDSTS_SHORT to and including DDSTS_ANY.
 * @param[out]  result  Pointer to the created ddsts_base_type_t struct.
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
ddsts_create_base_type(ddsts_flags_t flags, ddsts_type_t **result);

/* Sequence type (sequence_type) */
typedef struct {
  ddsts_typespec_t typespec;
  ddsts_type_t *element_type;
  unsigned long long max;
} ddsts_sequence_t;

/**
 * @brief Creates a ddsts_sequence_t struct with a given element type.
 *
 * @param[in]   element_type  A non-NULL pointer to the element type. If
 *                            function returns success and the element type
 *                            is not owned yet, it will be is owned by the
 *                            created ddsts_sequence_t struct.
 * @param[in]   max           A positive number representing the maximum size
 *                            of the sequence or 0 for representing an unbound
 *                            sequence.
 * @param[out]  result        Pointer to the created ddsts_sequence_t struct.
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
ddsts_create_sequence(ddsts_type_t *element_type, unsigned long long max, ddsts_type_t **result);

/* Array type */
typedef struct {
  ddsts_typespec_t typespec;
  ddsts_type_t *element_type;
  unsigned long long size;
} ddsts_array_t;

/**
 * @brief Creates a ddsts_array_t struct with a given element type.
 *
 * @param[in]   element_type  A possibly NULL pointer to the element type. If
 *                            the pointer was non-NULL, not yet owned, and the
 *                            function returns, it will be owned by the created
 *                            ddsts_array_t struct.
 * @param[in]   size          A positive number representing the size of the
 *                            array.
 * @param[out]  result        Pointer to the created ddsts_array_t struct.
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
ddsts_create_array(ddsts_type_t *element_type, unsigned long long size, ddsts_type_t **result);

/**
 * @brief Sets the element type of a ddsts_array_t.
 *
 * @param[inout] array         A non-NULL pointer to an ddsts_array_t struct
 *                             with a NULL element type.
 * @param[in]    element_type  A non-NULL pointer to the element type. If the
 *                             function returns success and the element type
 *                             is not owned yet, it will be owned by the created
 *                             ddsts_array_t struct.
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
ddsts_array_set_element_type(ddsts_type_t *array, ddsts_type_t *element_type);

/* (Wide) string type (string_type, wide_string_type) */
typedef struct {
  ddsts_typespec_t typespec;
  unsigned long long max;
} ddsts_string_t;

/**
 * @brief Creates a ddsts_string_t struct.
 *
 * @param[in]   flags    One of DDSTS_STRING or DDSTS_WIDE_STRING.
 * @param[in]   max      A positive number representing the maximum size
 *                       of the string or 0 for representing an unbound
 *                       string.
 * @param[out]  result   Pointer to the created ddsts_string_t struct.
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
ddsts_create_string(ddsts_flags_t flags, unsigned long long max, ddsts_type_t **result);

/* Fixed point type (fixed_pt_type) */
typedef struct {
  ddsts_typespec_t typespec;
  unsigned long long digits;
  unsigned long long fraction_digits;
} ddsts_fixed_pt_t;

/**
 * @brief Creates a ddsts_fixed_pt_t struct.
 *
 * @param[in]   digits           A positive number representing the maximum
 *                               number of digits.
 * @param[in]   fraction_digits  A positive number representing the number of
 *                               fraction digits.
 * @param[out]  result           Pointer to the created ddsts_fixed_pt_t struct.
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
ddsts_create_fixed_pt(unsigned long long digits, unsigned long long fraction_digits, ddsts_type_t **result);

/* Map type (map_type) */
typedef struct {
  ddsts_typespec_t typespec;
  ddsts_type_t *key_type;
  ddsts_type_t *value_type;
  unsigned long long max;
} ddsts_map_t;

/**
 * @brief Creates a ddsts_map_t struct with a given element type.
 *
 * @param[in]   key_type    A non-NULL pointer to the key type. If the function
 *                          returns success and the key type was not owned yet,
 *                          it will be owned by the created ddsts_map_t struct.
 * @param[in]   value_type  A non-NULL pointer to the key type. If the function
 *                          returns success and the value type was not owned yet,
 *                          it will be owned by the created ddsts_map_t struct.
 * @param[in]   max         A positive number representing the maximum size
 *                          of the map or 0 for representing an unbound
 *                          map.
 * @param[out]  result      Pointer to the created ddsts_map_t struct.
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
ddsts_create_map(ddsts_type_t *key_type, ddsts_type_t *value_type, unsigned long long max, ddsts_type_t **result);

/* Module declaration (module_dcl)
 */
typedef struct ddsts_module ddsts_module_t;
struct ddsts_module {
  ddsts_typespec_t type;
  ddsts_type_t *members;
  ddsts_module_t *previous; /* to previous open of this module, if present */
};

/**
 * @brief Creates a ddsts_module_t with no members.
 *
 * @param[in]   name    A non-NULL pointer to a string. If the function
 *                      returns success, the string should be considered as
 *                      owned by the created ddsts_module_t struct.
 * @param[out]  result  Pointer to the created ddsts_module_t struct.
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
ddsts_create_module(ddsts_identifier_t name, ddsts_type_t **result);

/**
 * @brief Adds a member at the end of the members of a ddsts_module_t struct.
 *
 * @param[inout] module  A non-NULL pointer to a ddsts_module_t struct.
 * @param[in]    member  A non-NULL pointer to a member type that is not owned
 *                       yet. If the function returns success, the member type
 *                       will be owned by the module.
 *                       In case the member type is a module and the module
 *                       is reopened the 'previous' struct member is set to
 *                       point to the previous opening of the module.
 *                       In case the member type is a struct, the 'definition'
 *                       struct member of matching previous forward declaration
 *                       are set to this struct.
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
ddsts_module_add_member(ddsts_type_t *module, ddsts_type_t *member);

/* Forward declaration */
typedef struct {
  ddsts_typespec_t type;
  ddsts_type_t *definition; /* reference to the actual definition */
} ddsts_forward_t;

/* Struct forward declaration (struct_forward_dcl)
 * ddsts_forward_t (no extra members)
 */

/**
 * @brief Creates a ddsts_forward_t struct for a struct forward declaration.
 *
 * @param[in]   name    A non-NULL pointer to a string. If the function
 *                      returns success, the string should be considered as
 *                      owned by the created ddsts_forward_t struct.
 * @param[out]  result  Pointer to the created ddsts_forward_t struct.
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
ddsts_create_struct_forward_dcl(ddsts_identifier_t name, ddsts_type_t **result);

/* Struct declaration (struct_def)
 */
typedef struct {
  ddsts_typespec_t type;
  ddsts_type_t *super; /* used for extended struct type definition */
  ddsts_type_t *members;
} ddsts_struct_t;

/**
 * @brief Creates a ddsts_struct_t with no members.
 *
 * @param[in]   name    A non-NULL pointer to a string. If the function
 *                      returns success, the string should be considered as
 *                      owned by the created ddsts_struct_t struct.
 * @param[out]  result  Pointer to the created ddsts_struct_t struct.
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
ddsts_create_struct(ddsts_identifier_t name, ddsts_type_t **result);

/**
 * @brief Adds a member at the end of the members of a ddsts_struct_t struct.
 *
 * @param[inout] module  A non-NULL pointer to a ddsts_struct_t struct.
 * @param[in]    member  A non-NULL pointer to a member type that is not owned
 *                       yet. If the function returns success, the member type
 *                       will be owned by the struct.
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
ddsts_struct_add_member(ddsts_type_t *struct_def, ddsts_type_t *member);

/* Declaration
 */
typedef struct {
  ddsts_typespec_t type;
  ddsts_type_t *decl_type;
} ddsts_declaration_t;

/**
 * @brief Creates a ddsts_declaration_t struct.
 *
 * @param[in]   name       A non-NULL pointer to a string. If the function
 *                         returns success, the string should be considered as
 *                         owned by the created ddsts_declaration_t struct.
 * @param[in]   decl_type  A possibly NULL pointer to the type of the declaration.
 *                         If the pointer was non-NULL, not yet owned, and the
 *                         function returns, it will be owned by the created
 *                         ddsts_declaration_t struct.
 * @param[out]  result     Pointer to the created ddsts_declaration_t struct.
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
ddsts_create_declaration(ddsts_identifier_t name, ddsts_type_t *decl_type, ddsts_type_t **result);

/**
 * @brief Sets the declaration type of a ddsts_declaration_t.
 *
 * @param[inout] array         A non-NULL pointer to an ddsts_declaration_t
 *                             struct with a NULL element type.
 * @param[in]    element_type  A non-NULL pointer to the declaration type. If the
 *                             function returns success and the element type
 *                             is not owned yet, it will be owned by the created
 *                             ddsts_declaration_t struct.
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
ddsts_declaration_set_type(ddsts_type_t *declaration, ddsts_type_t *type);


/* The union of all type specs */
union ddsts_type {
  ddsts_typespec_t type;
  ddsts_base_type_t base_type;
  ddsts_sequence_t sequence;
  ddsts_array_t array;
  ddsts_string_t string;
  ddsts_fixed_pt_t fixed_pt;
  ddsts_map_t map;
  ddsts_module_t module;
  ddsts_forward_t forward;
  ddsts_struct_t struct_def;
  ddsts_declaration_t declaration;
};

#endif /* DDS_TYPE_TYPETREE_H */
