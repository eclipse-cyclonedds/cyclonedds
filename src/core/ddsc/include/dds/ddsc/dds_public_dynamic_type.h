// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS_DYNAMIC_TYPE_H
#define DDS_DYNAMIC_TYPE_H

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_typeinfo;

/**
 * @defgroup dynamic_type (Dynamic Type API)
 * @ingroup dds
 * The Dynamic Type API to construct and manipulate data types.
 */

/**
 * @brief Dynamic Type
 * @ingroup dynamic_type
 *
 * Representation of a dynamically created type. This struct has an opaque pointer
 * to the type in the type system. During construction of the type (setting properties
 * and adding members), the internal type has the state 'CONSTRUCTION'. Once the type
 * is registered, the state is updated to 'RESOLVED' and the type cannot be modified.
 *
 * The 'ret' member of this struct holds the return code of operations performed on
 * this type. In case this value is not DDS_RETCODE_OK, the type cannot be used for
 * further processing (e.g. adding members, registering the type, etc.).
 *
 */
typedef struct dds_dynamic_type {
  void * x;
  dds_return_t ret;
} dds_dynamic_type_t;

/**
 * @ingroup dynamic_type
 *
 * Invalid member ID: used when adding a member, to indicate that the member should get
 * the id (m+1) where m is the highest member id in the current set of members. A valid
 * member id has the 4 most significant bits set to 0 (because of usage in the EMHEADER),
 * so also when hashed-id are used, the hash member id will never be set to the invalid
 * member id.
 */
#define DDS_DYNAMIC_MEMBER_ID_INVALID 0xf000000u
#define DDS_DYNAMIC_MEMBER_ID_AUTO DDS_DYNAMIC_MEMBER_ID_INVALID

/**
 * @ingroup dynamic_type
 *
 * When adding members, index 0 is used to indicate that a member should be inserted as
 * the first member. A value higher than the current maximum index can be used to indicate
 * that the member should be added after the other members
 */
#define DDS_DYNAMIC_MEMBER_INDEX_START 0u
#define DDS_DYNAMIC_MEMBER_INDEX_END UINT32_MAX

/**
 * @brief Dynamic Type Kind
 * @ingroup dynamic_type
 *
 * Enumeration with the type kind values that can be used to create a dynamic type.
 */
typedef enum dds_dynamic_type_kind
{
  DDS_DYNAMIC_NONE,
  DDS_DYNAMIC_BOOLEAN,
  DDS_DYNAMIC_BYTE,
  DDS_DYNAMIC_INT16,
  DDS_DYNAMIC_INT32,
  DDS_DYNAMIC_INT64,
  DDS_DYNAMIC_UINT16,
  DDS_DYNAMIC_UINT32,
  DDS_DYNAMIC_UINT64,
  DDS_DYNAMIC_FLOAT32,
  DDS_DYNAMIC_FLOAT64,
  DDS_DYNAMIC_FLOAT128,
  DDS_DYNAMIC_INT8,
  DDS_DYNAMIC_UINT8,
  DDS_DYNAMIC_CHAR8,
  DDS_DYNAMIC_CHAR16,
  DDS_DYNAMIC_STRING8,
  DDS_DYNAMIC_STRING16,
  DDS_DYNAMIC_ENUMERATION,
  DDS_DYNAMIC_BITMASK,
  DDS_DYNAMIC_ALIAS,
  DDS_DYNAMIC_ARRAY,
  DDS_DYNAMIC_SEQUENCE,
  DDS_DYNAMIC_MAP,
  DDS_DYNAMIC_STRUCTURE,
  DDS_DYNAMIC_UNION,
  DDS_DYNAMIC_BITSET
} dds_dynamic_type_kind_t;

/**
 * @ingroup dynamic_type
 *
 * Short notation for initializer of a dynamic type spec for non-primitive and primitive types.
 */
#define DDS_DYNAMIC_TYPE_SPEC(t) ((dds_dynamic_type_spec_t) { .kind = DDS_DYNAMIC_TYPE_KIND_DEFINITION, .type = { .type = (t) } })
#define DDS_DYNAMIC_TYPE_SPEC_PRIM(p) ((dds_dynamic_type_spec_t) { .kind = DDS_DYNAMIC_TYPE_KIND_PRIMITIVE, .type = { .primitive = (p) } })

/**
 * @ingroup dynamic_type
 *
 * Short notation for struct member descriptor with different sets of commonly used properties
 */
#define DDS_DYNAMIC_MEMBER_(member_type_spec,member_name,member_id,member_index) \
    ((dds_dynamic_member_descriptor_t) { \
      .name = (member_name), \
      .id = (member_id), \
      .type = (member_type_spec), \
      .index = (member_index) \
    })
#define DDS_DYNAMIC_MEMBER_ID(member_type,member_name,member_id) \
    DDS_DYNAMIC_MEMBER_ (DDS_DYNAMIC_TYPE_SPEC((member_type)),(member_name),(member_id),DDS_DYNAMIC_MEMBER_INDEX_END)
#define DDS_DYNAMIC_MEMBER_ID_PRIM(member_prim_type,member_name,member_id) \
    DDS_DYNAMIC_MEMBER_(DDS_DYNAMIC_TYPE_SPEC_PRIM((member_prim_type)),(member_name),(member_id),DDS_DYNAMIC_MEMBER_INDEX_END)
#define DDS_DYNAMIC_MEMBER(member_type,member_name) \
    DDS_DYNAMIC_MEMBER_ID((member_type),(member_name),DDS_DYNAMIC_MEMBER_ID_INVALID)
#define DDS_DYNAMIC_MEMBER_PRIM(member_prim_type,member_name) \
    DDS_DYNAMIC_MEMBER_ID_PRIM((member_prim_type),(member_name),DDS_DYNAMIC_MEMBER_ID_INVALID)

/**
 * @ingroup dynamic_type
 *
 * Short notation for union member descriptor with different sets of commonly used properties
 */
#define DDS_DYNAMIC_UNION_MEMBER_(member_type_spec,member_name,member_id,member_index,member_num_labels,member_labels,member_is_default) \
    ((dds_dynamic_member_descriptor_t) { \
      .name = (member_name), \
      .id = (member_id), \
      .type = (member_type_spec), \
      .index = (member_index), \
      .num_labels = (member_num_labels), \
      .labels = (member_labels), \
      .default_label = (member_is_default) \
    })
#define DDS_DYNAMIC_UNION_MEMBER_ID(member_type,member_name,member_id,member_num_labels,member_labels) \
    DDS_DYNAMIC_UNION_MEMBER_(DDS_DYNAMIC_TYPE_SPEC((member_type)),(member_name),(member_id),DDS_DYNAMIC_MEMBER_INDEX_END,(member_num_labels),(member_labels),false)
#define DDS_DYNAMIC_UNION_MEMBER_ID_PRIM(member_prim_type,member_name,member_id,member_num_labels,member_labels) \
    DDS_DYNAMIC_UNION_MEMBER_(DDS_DYNAMIC_TYPE_SPEC_PRIM((member_prim_type)),(member_name),(member_id),DDS_DYNAMIC_MEMBER_INDEX_END,(member_num_labels),(member_labels),false)
#define DDS_DYNAMIC_UNION_MEMBER(member_type,member_name,member_num_labels,member_labels) \
    DDS_DYNAMIC_UNION_MEMBER_ID((member_type),(member_name),DDS_DYNAMIC_MEMBER_ID_INVALID,(member_num_labels),(member_labels))
#define DDS_DYNAMIC_UNION_MEMBER_PRIM(member_prim_type,member_name,member_num_labels,member_labels) \
    DDS_DYNAMIC_UNION_MEMBER_ID_PRIM((member_prim_type),(member_name),DDS_DYNAMIC_MEMBER_ID_INVALID,(member_num_labels),(member_labels))

#define DDS_DYNAMIC_UNION_MEMBER_DEFAULT_ID(member_type,member_name,member_id) \
    DDS_DYNAMIC_UNION_MEMBER_(DDS_DYNAMIC_TYPE_SPEC((member_type)),(member_name),(member_id),DDS_DYNAMIC_MEMBER_INDEX_END,0,NULL,true)
#define DDS_DYNAMIC_UNION_MEMBER_DEFAULT_ID_PRIM(member_prim_type,member_name,member_id) \
    DDS_DYNAMIC_UNION_MEMBER_(DDS_DYNAMIC_TYPE_SPEC_PRIM((member_prim_type)),(member_name),(member_id),DDS_DYNAMIC_MEMBER_INDEX_END,0,NULL,true)
#define DDS_DYNAMIC_UNION_MEMBER_DEFAULT(member_type,member_name) \
    DDS_DYNAMIC_UNION_MEMBER_DEFAULT_ID((member_type),(member_name),DDS_DYNAMIC_MEMBER_ID_INVALID)
#define DDS_DYNAMIC_UNION_MEMBER_DEFAULT_PRIM(member_prim_type,member_name) \
    DDS_DYNAMIC_UNION_MEMBER_DEFAULT_ID_PRIM((member_prim_type),(member_name),DDS_DYNAMIC_MEMBER_ID_INVALID)

/**
 * @ingroup dynamic_type
 *
 * Dynamic Type specification kind
 */
typedef enum dds_dynamic_type_spec_kind {
  DDS_DYNAMIC_TYPE_KIND_UNSET,
  DDS_DYNAMIC_TYPE_KIND_DEFINITION,
  DDS_DYNAMIC_TYPE_KIND_PRIMITIVE
} dds_dynamic_type_spec_kind_t;

/**
 * @ingroup dynamic_type
 *
 * Dynamic Type specification: a reference to dynamic type, which can be a primitive type
 * kind (just the type kind enumeration value), or a (primitive or non-primitive) dynamic
 * type reference.
 */
typedef struct dds_dynamic_type_spec {
  dds_dynamic_type_spec_kind_t kind;
  union {
    dds_dynamic_type_t type;
    dds_dynamic_type_kind_t primitive;
  } type;
} dds_dynamic_type_spec_t;

/**
 * @brief Dynamic Type descriptor
 * @ingroup dynamic_type
 *
 * Structure that holds the properties for creating a Dynamic Type. For each type kind,
 * specific member fields are applicable and/or required.
 */
typedef struct dds_dynamic_type_descriptor {
  dds_dynamic_type_kind_t kind; /**< Type kind. Required for all types. */
  const char * name; /**< Type name. Required for struct, union, alias, enum, bitmask, array, sequence. */
  dds_dynamic_type_spec_t base_type; /**< Option base type for a struct, or (required) aliased type in case of an alias type. */
  dds_dynamic_type_spec_t discriminator_type; /**< Discriminator type for a union (required). */
  uint32_t num_bounds; /**< Number of bounds for array and sequence types. In case of sequence, this can be 0 (unbounded) or 1. */
  const uint32_t *bounds; /**< Bounds for array (0..num_bounds) and sequence (single value) */
  dds_dynamic_type_spec_t element_type; /**< Element type for array and sequence, required. */
  dds_dynamic_type_spec_t key_element_type; /**< Key element type for map type */
} dds_dynamic_type_descriptor_t;

/**
 * @brief Dynamic Type Member descriptor
 * @ingroup dynamic_type
 *
 * Structure that holds the properities for adding a member to a dynamic type. Depending on
 * the member type, different fields apply and are required.
 */
typedef struct dds_dynamic_member_descriptor {
  const char * name; /**< Name of the member, required */
  uint32_t id; /**< Identifier of the member, applicable for struct and union members. DDS_DYNAMIC_MEMBER_ID_AUTO can be used to indicate the next available id (current max + 1) should be used. */
  dds_dynamic_type_spec_t type; /**< Member type, required for struct and union members. */
  char *default_value; /**< Default value for the member */
  uint32_t index; /**< Member index, applicable for struct and union members. DDS_DYNAMIC_MEMBER_INDEX_START and DDS_DYNAMIC_MEMBER_INDEX_END can be used to add a member as first or last member in the parent type. */
  uint32_t num_labels; /**< Number of labels, required for union members in case not default_label */
  int32_t *labels; /**< Labels for a union member, 1..n required for union members in case not default_label */
  bool default_label; /**< Is default union member */
} dds_dynamic_member_descriptor_t;

/**
 * @ingroup dynamic_type
 *
 * Dynamic Type extensibility
 */
enum dds_dynamic_type_extensibility {
  DDS_DYNAMIC_TYPE_EXT_FINAL,
  DDS_DYNAMIC_TYPE_EXT_APPENDABLE,
  DDS_DYNAMIC_TYPE_EXT_MUTABLE
};

/**
 * @ingroup dynamic_type
 *
 * Dynamic Type automatic member ID kind
 */
enum dds_dynamic_type_autoid {
  DDS_DYNAMIC_TYPE_AUTOID_SEQUENTIAL, /**< The member ID are assigned sequential */
  DDS_DYNAMIC_TYPE_AUTOID_HASH /**< The member ID is the hash of the member's name */
};

/**
 * @brief Enum value kind
 *
 * see @ref dds_dynamic_enum_literal_value
 */
enum dds_dynamic_type_enum_value_kind {
    DDS_DYNAMIC_ENUM_LITERAL_VALUE_NEXT_AVAIL,
    DDS_DYNAMIC_ENUM_LITERAL_VALUE_EXPLICIT
};

/**
 * @ingroup dynamic_type
 *
 * Dynamic Enumeration type literal value kind and value. Can be set to NEXT_AVAIL to indicate
 * that the current max value + 1 should be used for this member, or an explicit value can be
 * provided.
 */
typedef struct dds_dynamic_enum_literal_value {
  enum dds_dynamic_type_enum_value_kind value_kind;
  int32_t value;
} dds_dynamic_enum_literal_value_t;

/**
 * @ingroup dynamic_type
 *
 * Short notation for initializing a Dynamic Enum value struct.
 */
#define DDS_DYNAMIC_ENUM_LITERAL_VALUE_AUTO ((dds_dynamic_enum_literal_value_t) { DDS_DYNAMIC_ENUM_LITERAL_VALUE_NEXT_AVAIL, 0 })
#define DDS_DYNAMIC_ENUM_LITERAL_VALUE(v) ((dds_dynamic_enum_literal_value_t) { DDS_DYNAMIC_ENUM_LITERAL_VALUE_EXPLICIT, (v) })

/**
 * @ingroup dynamic_type
 *
 * Used to indicate that the bitmask field should get the next available position (current maximum + 1)
 */
#define DDS_DYNAMIC_BITMASK_POSITION_AUTO (UINT16_MAX)

/**
 * @brief Create a new Dynamic Type
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * Creates a new Dynamic Type, using the properties that are set in the type descriptor.
 * In case these properties include a base-type, element-type or discriminator type, the
 * ownership of these types is transferred to the newly created type.
 *
 * @param[in] entity A DDS entity (any entity, except the pseudo root entity identified by DDS_CYCLONEDDS_HANDLE). This entity is used to get the type library of the entity's domain, to add the type to.
 * @param[in] descriptor The Dynamic Type descriptor.
 *
 * @return dds_dynamic_type_t A Dynamic Type reference for the created type.
 *
 * @retval DDS_RETCODE_OK
 *            The type is created successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_UNSUPPORTED
 *            The provided type kind is not supported.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES
 *            Not enough resources to create the type.
 */
DDS_EXPORT dds_dynamic_type_t dds_dynamic_type_create (dds_entity_t entity, dds_dynamic_type_descriptor_t descriptor);

/**
 * @brief Set the extensibility of a Dynamic Type
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * @param[in,out] type Dynamic Type to set the extensibility for. This can be a structure, union, bitmask or enum type. This type must be in the CONSTRUCTING state and have no members added.
 * @param[in] extensibility The extensibility to set (@ref enum dds_dynamic_type_extensibility).
 *
 * @return dds_return_t Return code. In case of an error, the return code field in the provided type is also set to this value.
 *
 * @retval DDS_RETCODE_OK
 *            The extensibility is set successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *            The provided type is not in the CONSTRUCTING state.
 */
DDS_EXPORT dds_return_t dds_dynamic_type_set_extensibility (dds_dynamic_type_t *type, enum dds_dynamic_type_extensibility extensibility);

/**
 * @brief Set the bit-bound of a Dynamic Type
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * @param[in,out] type Dynamic Type to set the bit-bound for. This can be a bitmask or enum type.
 * @param[in] bit_bound The bit-bound value to set, in the (including) range 1..32 for enum and 1..64 for bitmask.
 *
 * @return dds_return_t Return code. In case of an error, the return code field in the provided type is also set to this value.
 *
 * @retval DDS_RETCODE_OK
 *            The bit-bound is set successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *            The provided type is not in the CONSTRUCTING state.
 */
DDS_EXPORT dds_return_t dds_dynamic_type_set_bit_bound (dds_dynamic_type_t *type, uint16_t bit_bound);

/**
 * @brief Set the nested flag of a Dynamic Type
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * @param[in,out] type Dynamic Type to set the nested flag for. This can be a structure or union type.
 * @param[in] is_nested Whether the nested flag is set.
 *
 * @return dds_return_t Return code. In case of an error, the return code field in the provided type is also set to this value.
 *
 * @retval DDS_RETCODE_OK
 *            The flag is set successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *            The provided type is not in the CONSTRUCTING state.
 */
DDS_EXPORT dds_return_t dds_dynamic_type_set_nested (dds_dynamic_type_t *type, bool is_nested);

/**
 * @brief Set the auto-id kind of a Dynamic Type
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * @param[in,out] type Dynamic Type to set the auto-id kind for. This can be a structure or union type.
 * @param[in] value The auto-id kind, see @ref dds_dynamic_type_autoid.
 *
 * @return dds_return_t Return code. In case of an error, the return code field in the provided type is also set to this value.
 *
 * @retval DDS_RETCODE_OK
 *            The value is set successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *            The provided type is not in the CONSTRUCTING state.
 */
DDS_EXPORT dds_return_t dds_dynamic_type_set_autoid (dds_dynamic_type_t *type, enum dds_dynamic_type_autoid value);

/**
 * @brief Add a member to a Dynamic Type
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * This function is used to add a member to a Dynamic Type. The parent type can be a structure,
 * union, enumeration or bitmask type. The parent type the member is added to takes over the
 * ownership of the member type and dereferences the member type when it is deleted.
 * (@see dds_dynamic_type_ref for re-using a type)
 *
 * @param[in,out] type The Dynamic type to add the member to.
 * @param[in] member_descriptor The member descriptor that has the properties of the member to add, @see dds_dynamic_member_descriptor.
 *
 * @return dds_return_t Return code. In case of an error, the return code field in the provided type is also set to this value.
 *
 * @retval DDS_RETCODE_OK
 *            The member is added successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *            The provided type (non the member type) is not in the CONSTRUCTING state.
 */
DDS_EXPORT dds_return_t dds_dynamic_type_add_member (dds_dynamic_type_t *type, dds_dynamic_member_descriptor_t member_descriptor);

/**
 * @brief Add a literal to a Dynamic Enum Type
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * This function is used to add a literal to a Dynamic Enum Type.
 *
 * @param[in,out] type The Dynamic enum type to add the member to.
 * @param[in] name The name of the literal to add.
 * @param[in] value The value for the literal (@see dds_dynamic_enum_literal_value).
 * @param[in] is_default Indicates if the literal if default for the enum.
 *
 * @return dds_return_t Return code. In case of an error, the return code field in the provided type is also set to this value.
 *
 * @retval DDS_RETCODE_OK
 *            The member is added successfully
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *            The provided type is not in the CONSTRUCTING state.
 */
DDS_EXPORT dds_return_t dds_dynamic_type_add_enum_literal (dds_dynamic_type_t *type, const char *name, dds_dynamic_enum_literal_value_t value, bool is_default);

/**
 * @brief Add a field to a Dynamic bitmask Type
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * This function is used to add a field to a Dynamic bitmask Type.
 *
 * @param[in,out] type The Dynamic bitmask type to add the field to.
 * @param[in] name The name of the field to add.
 * @param[in] position The position for the field (@see DDS_DYNAMIC_BITMASK_POSITION_AUTO).
 *
 * @return dds_return_t Return code. In case of an error, the return code field in the provided type is also set to this value.
 *
 * @retval DDS_RETCODE_OK
 *            The member is added successfully
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *            The provided type is not in the CONSTRUCTING state.
 */
DDS_EXPORT dds_return_t dds_dynamic_type_add_bitmask_field (dds_dynamic_type_t *type, const char *name, uint16_t position);

/**
 * @brief Set the key flag for a Dynamic Type member
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * @param[in,out] type Dynamic Type that contains the member to set the key flag for (must be a structure type).
 * @param[in] member_id The ID of the member to set the flag for.
 * @param[in] is_key Indicates whether the key flag should be set or cleared.
 *
 * @return dds_return_t Return code. In case of an error, the return code field in the provided type is also set to this value.
 *
 * @retval DDS_RETCODE_OK
 *            The flag is updated successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *            The provided type is not in the CONSTRUCTING state.
 */
DDS_EXPORT dds_return_t dds_dynamic_member_set_key (dds_dynamic_type_t *type, uint32_t member_id, bool is_key);

/**
 * @brief Set the optional flag for a Dynamic Type member
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * @param[in,out] type Dynamic Type that contains the member to set the optional flag for (must be a structure type).
 * @param[in] member_id The ID of the member to set the flag for.
 * @param[in] is_optional Indicates whether the optional flag should be set or cleared.
 *
 * @return dds_return_t Return code. In case of an error, the return code field in the provided type is also set to this value.
 *
 * @retval DDS_RETCODE_OK
 *            The flag is updated successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *            The provided type is not in the CONSTRUCTING state.
 */
DDS_EXPORT dds_return_t dds_dynamic_member_set_optional (dds_dynamic_type_t *type, uint32_t member_id, bool is_optional);

/**
 * @brief Set the external flag for a Dynamic Type member
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * @param[in,out] type Dynamic Type that contains the member to set the external flag for (must be a structure or union type).
 * @param[in] member_id The ID of the member to set the flag for.
 * @param[in] is_external Indicates whether the external flag should be set or cleared.
 *
 * @return dds_return_t Return code. In case of an error, the return code field in the provided type is also set to this value.
 *
 * @retval DDS_RETCODE_OK
 *            The flag is updated successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *            The provided type is not in the CONSTRUCTING state.
 */
DDS_EXPORT dds_return_t dds_dynamic_member_set_external (dds_dynamic_type_t *type, uint32_t member_id, bool is_external);

/**
 * @brief Set the hash ID flag and hash field name for a Dynamic Type member
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * @param[in,out] type Dynamic Type that contains the member to set the flag and hash-name for (must be a structure or union type).
 * @param[in] member_id The ID of the member to set the flag and hash-name for.
 * @param[in] hash_member_name The hash-name that should be used for calculating the member ID.
 *
 * @return dds_return_t Return code. In case of an error, the return code field in the provided type is also set to this value.
 *
 * @retval DDS_RETCODE_OK
 *            The flag is updated successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *            The provided type is not in the CONSTRUCTING state.
 */
DDS_EXPORT dds_return_t dds_dynamic_member_set_hashid (dds_dynamic_type_t *type, uint32_t member_id, const char *hash_member_name);

/**
 * @brief Set the must-understand flag for a Dynamic Type member
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * @param[in,out] type Dynamic Type that contains the member to set the must-understand flag for (must be a structure type).
 * @param[in] member_id The ID of the member to set the flag for.
 * @param[in] is_must_understand Indicates whether the must-understand flag should be set or cleared.
 *
 * @return dds_return_t Return code. In case of an error, the return code field in the provided type is also set to this value.
 *
 * @retval DDS_RETCODE_OK
 *            The flag is updated successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *            The provided type is not in the CONSTRUCTING state.
 */
DDS_EXPORT dds_return_t dds_dynamic_member_set_must_understand (dds_dynamic_type_t *type, uint32_t member_id, bool is_must_understand);


/**
 * @brief Registers a Dynamic Type
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * This function registers a dynamic type, making it immutable and finalizing
 * its definition. A type that is registered, get the state 'RESOLVED' and is
 * stored in the type library.
 *
 * @param[in] type A pointer to the dynamic type to be registered.
 * @param[out] type_info A pointer to a pointer to a ddsi_typeinfo structure that holds information about the registered type.
 *
 * @return dds_return_t Return code.
 *
 * @retval DDS_RETCODE_OK
 *            The type was successfully registered.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *            The provided type is not in the CONSTRUCTING state.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES
 *            Not enough resources to create the type.
 */
DDS_EXPORT dds_return_t dds_dynamic_type_register (dds_dynamic_type_t *type, struct ddsi_typeinfo **type_info);

/**
 * @brief Reference a Dynamic Type
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * References a Dynamic Type and increases the ref-count of the type. This
 * can e.g. be used to re-use a subtype when constructing a type.
 *
 * @param type Dynamic Type to reference
 *
 * @return dds_dynamic_type_t Dynamic Type with increased ref-count
 */
DDS_EXPORT dds_dynamic_type_t dds_dynamic_type_ref (dds_dynamic_type_t *type);

/**
 * @brief Unref a Dynamic Type
 *
 * @param type The Dynamic Type to dereference.
 *
 * @return dds_return_t Return code.
 *
 * @retval DDS_RETCODE_OK
 *            The type was successfully registered.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One or more of the provided parameters are invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *            The provided type is not in the CONSTRUCTING state.
 */
DDS_EXPORT dds_return_t dds_dynamic_type_unref (dds_dynamic_type_t *type);

/**
 * @brief Duplicate a Dynamic Type
 * @ingroup dynamic_type
 * @component dynamic_type_api
 *
 * Duplicates a Dynamic Type. Dependencies of the type are not duplicated,
 * but their ref-count is increased.
 *
 * @param src The type to duplicate.
 *
 * @return dds_dynamic_type_t A duplicate of the source type.
 */
DDS_EXPORT dds_dynamic_type_t dds_dynamic_type_dup (const dds_dynamic_type_t *src);


#if defined (__cplusplus)
}
#endif

#endif // DDS_DYNAMIC_TYPE_H
