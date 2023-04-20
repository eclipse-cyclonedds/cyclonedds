// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>
#include "dds/dds.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_dynamic_type.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds__entity.h"

static dds_return_t get_entity_gv (dds_entity_t entity, struct ddsi_domaingv **gv)
{
  dds_return_t ret;
  struct dds_entity *e;
  if ((ret = dds_entity_pin (entity, &e)) == DDS_RETCODE_OK)
  {
    if (e->m_kind == DDS_KIND_CYCLONEDDS)
      ret = DDS_RETCODE_BAD_PARAMETER;
    else
      *gv = &e->m_domain->gv;
    dds_entity_unpin (e);
  }
  return ret;
}

static DDS_XTypes_TypeKind typekind_to_xtkind (dds_dynamic_type_kind_t type_kind)
{
  switch (type_kind) {
    case DDS_DYNAMIC_NONE:        return DDS_XTypes_TK_NONE;
    case DDS_DYNAMIC_BOOLEAN:     return DDS_XTypes_TK_BOOLEAN;
    case DDS_DYNAMIC_BYTE:        return DDS_XTypes_TK_BYTE;
    case DDS_DYNAMIC_INT16:       return DDS_XTypes_TK_INT16;
    case DDS_DYNAMIC_INT32:       return DDS_XTypes_TK_INT32;
    case DDS_DYNAMIC_INT64:       return DDS_XTypes_TK_INT64;
    case DDS_DYNAMIC_UINT16:      return DDS_XTypes_TK_UINT16;
    case DDS_DYNAMIC_UINT32:      return DDS_XTypes_TK_UINT32;
    case DDS_DYNAMIC_UINT64:      return DDS_XTypes_TK_UINT64;
    case DDS_DYNAMIC_FLOAT32:     return DDS_XTypes_TK_FLOAT32;
    case DDS_DYNAMIC_FLOAT64:     return DDS_XTypes_TK_FLOAT64;
    case DDS_DYNAMIC_FLOAT128:    return DDS_XTypes_TK_FLOAT128;
    case DDS_DYNAMIC_INT8:        return DDS_XTypes_TK_INT8;
    case DDS_DYNAMIC_UINT8:       return DDS_XTypes_TK_UINT8;
    case DDS_DYNAMIC_CHAR8:       return DDS_XTypes_TK_CHAR8;
    case DDS_DYNAMIC_CHAR16:      return DDS_XTypes_TK_CHAR16;
    case DDS_DYNAMIC_STRING8:     return DDS_XTypes_TK_STRING8;
    case DDS_DYNAMIC_STRING16:    return DDS_XTypes_TK_STRING16;
    case DDS_DYNAMIC_ENUMERATION: return DDS_XTypes_TK_ENUM;
    case DDS_DYNAMIC_BITMASK:     return DDS_XTypes_TK_BITMASK;
    case DDS_DYNAMIC_ALIAS:       return DDS_XTypes_TK_ALIAS;
    case DDS_DYNAMIC_ARRAY:       return DDS_XTypes_TK_ARRAY;
    case DDS_DYNAMIC_SEQUENCE:    return DDS_XTypes_TK_SEQUENCE;
    case DDS_DYNAMIC_MAP:         return DDS_XTypes_TK_MAP;
    case DDS_DYNAMIC_STRUCTURE:   return DDS_XTypes_TK_STRUCTURE;
    case DDS_DYNAMIC_UNION:       return DDS_XTypes_TK_UNION;
    case DDS_DYNAMIC_BITSET:      return DDS_XTypes_TK_BITSET;
  }
  return DDS_XTypes_TK_NONE;
}

static dds_dynamic_type_kind_t xtkind_to_typekind (DDS_XTypes_TypeKind xt_kind)
{
  switch (xt_kind) {
    case DDS_XTypes_TK_BOOLEAN: return DDS_DYNAMIC_BOOLEAN;
    case DDS_XTypes_TK_BYTE: return DDS_DYNAMIC_BYTE;
    case DDS_XTypes_TK_INT16: return DDS_DYNAMIC_INT16;
    case DDS_XTypes_TK_INT32: return DDS_DYNAMIC_INT32;
    case DDS_XTypes_TK_INT64: return DDS_DYNAMIC_INT64;
    case DDS_XTypes_TK_UINT16: return DDS_DYNAMIC_UINT16;
    case DDS_XTypes_TK_UINT32: return DDS_DYNAMIC_UINT32;
    case DDS_XTypes_TK_UINT64: return DDS_DYNAMIC_UINT64;
    case DDS_XTypes_TK_FLOAT32: return DDS_DYNAMIC_FLOAT32;
    case DDS_XTypes_TK_FLOAT64: return DDS_DYNAMIC_FLOAT64;
    case DDS_XTypes_TK_FLOAT128: return DDS_DYNAMIC_FLOAT128;
    case DDS_XTypes_TK_INT8: return DDS_DYNAMIC_INT8;
    case DDS_XTypes_TK_UINT8: return DDS_DYNAMIC_UINT8;
    case DDS_XTypes_TK_CHAR8: return DDS_DYNAMIC_CHAR8;
    case DDS_XTypes_TK_CHAR16: return DDS_DYNAMIC_CHAR16;
    case DDS_XTypes_TK_STRING8: return DDS_DYNAMIC_STRING8;
    case DDS_XTypes_TK_STRING16: return DDS_DYNAMIC_STRING16;
    case DDS_XTypes_TK_ENUM: return DDS_DYNAMIC_ENUMERATION;
    case DDS_XTypes_TK_BITMASK: return DDS_DYNAMIC_BITMASK;
    case DDS_XTypes_TK_ALIAS: return DDS_DYNAMIC_ALIAS;
    case DDS_XTypes_TK_ARRAY: return DDS_DYNAMIC_ARRAY;
    case DDS_XTypes_TK_SEQUENCE: return DDS_DYNAMIC_SEQUENCE;
    case DDS_XTypes_TK_MAP: return DDS_DYNAMIC_MAP;
    case DDS_XTypes_TK_STRUCTURE: return DDS_DYNAMIC_STRUCTURE;
    case DDS_XTypes_TK_UNION: return DDS_DYNAMIC_UNION;
    case DDS_XTypes_TK_BITSET: return DDS_DYNAMIC_BITSET;
  }
  return DDS_DYNAMIC_NONE;
}

static dds_dynamic_type_t dyntype_from_typespec (struct ddsi_domaingv *gv, dds_dynamic_type_spec_t type_spec)
{
  switch (type_spec.kind)
  {
    case DDS_DYNAMIC_TYPE_KIND_UNSET:
      return (dds_dynamic_type_t) { .ret = DDS_RETCODE_OK };
    case DDS_DYNAMIC_TYPE_KIND_PRIMITIVE: {
      dds_dynamic_type_t type;
      type.ret = ddsi_dynamic_type_create_primitive (gv, (struct ddsi_type **) &type.x, typekind_to_xtkind (type_spec.type.primitive));
      return type;
    }
    case DDS_DYNAMIC_TYPE_KIND_DEFINITION:
      return type_spec.type.type;
  }

  return (dds_dynamic_type_t) { .ret = DDS_RETCODE_BAD_PARAMETER };
}

static bool typespec_valid (dds_dynamic_type_spec_t type_spec, bool allow_unset)
{
  switch (type_spec.kind)
  {
    case DDS_DYNAMIC_TYPE_KIND_UNSET:
      return allow_unset;
    case DDS_DYNAMIC_TYPE_KIND_PRIMITIVE:
      return type_spec.type.primitive >= DDS_DYNAMIC_BOOLEAN && type_spec.type.primitive <= DDS_DYNAMIC_CHAR16;
    case DDS_DYNAMIC_TYPE_KIND_DEFINITION:
      return type_spec.type.type.ret == DDS_RETCODE_OK && type_spec.type.type.x != NULL;
  }
  return false;
}

static bool union_disc_valid (dds_dynamic_type_spec_t type_spec)
{
  switch (type_spec.kind)
  {
    case DDS_DYNAMIC_TYPE_KIND_UNSET:
      return false;
    case DDS_DYNAMIC_TYPE_KIND_PRIMITIVE:
      return type_spec.type.primitive == DDS_DYNAMIC_BOOLEAN || type_spec.type.primitive == DDS_DYNAMIC_BYTE ||
        type_spec.type.primitive == DDS_DYNAMIC_INT8 || type_spec.type.primitive == DDS_DYNAMIC_INT16 || type_spec.type.primitive == DDS_DYNAMIC_INT32 || type_spec.type.primitive == DDS_DYNAMIC_INT64 ||
        type_spec.type.primitive == DDS_DYNAMIC_UINT8 || type_spec.type.primitive == DDS_DYNAMIC_UINT16 || type_spec.type.primitive == DDS_DYNAMIC_UINT32 || type_spec.type.primitive == DDS_DYNAMIC_UINT64 ||
        type_spec.type.primitive == DDS_DYNAMIC_CHAR8 || type_spec.type.primitive == DDS_DYNAMIC_CHAR16;
    case DDS_DYNAMIC_TYPE_KIND_DEFINITION: {
      if (type_spec.type.type.ret != DDS_RETCODE_OK || type_spec.type.type.x == NULL)
        return false;
      DDS_XTypes_TypeKind xtkind = ddsi_type_get_kind ((struct ddsi_type *) type_spec.type.type.x);
      return xtkind == DDS_XTypes_TK_ENUM || xtkind == DDS_XTypes_TK_ALIAS;
    }
  }
  return false;
}

static bool typename_valid (const char *name)
{
  size_t len = strlen (name);
  return len > 0 && len < (sizeof (DDS_XTypes_QualifiedTypeName) - 1);
}

static bool membername_valid (const char *name)
{
  size_t len = strlen (name);
  return len > 0 && len < (sizeof (DDS_XTypes_MemberName) - 1);
}

dds_dynamic_type_t dds_dynamic_type_create (dds_entity_t entity, dds_dynamic_type_descriptor_t descriptor)
{
  dds_dynamic_type_t type = { .x = NULL };
  struct ddsi_domaingv *gv;
  if ((type.ret = get_entity_gv (entity, &gv)) != DDS_RETCODE_OK)
    goto err;

  switch (descriptor.kind)
  {
    case DDS_DYNAMIC_NONE:
      goto err_bad_param;

    case DDS_DYNAMIC_BOOLEAN:
    case DDS_DYNAMIC_BYTE:
    case DDS_DYNAMIC_INT16:
    case DDS_DYNAMIC_INT32:
    case DDS_DYNAMIC_INT64:
    case DDS_DYNAMIC_UINT16:
    case DDS_DYNAMIC_UINT32:
    case DDS_DYNAMIC_UINT64:
    case DDS_DYNAMIC_FLOAT32:
    case DDS_DYNAMIC_FLOAT64:
    case DDS_DYNAMIC_FLOAT128:
    case DDS_DYNAMIC_INT8:
    case DDS_DYNAMIC_UINT8:
    case DDS_DYNAMIC_CHAR8:
      type.ret = ddsi_dynamic_type_create_primitive (gv, (struct ddsi_type **) &type.x, descriptor.kind);
      break;
    case DDS_DYNAMIC_STRING8:
      if (descriptor.num_bounds > 1)
        goto err_bad_param;
      type.ret = ddsi_dynamic_type_create_string8 (gv, (struct ddsi_type **) &type.x, descriptor.num_bounds ? descriptor.bounds[0] : 0);
      break;
    case DDS_DYNAMIC_ALIAS: {
      if (!typespec_valid (descriptor.base_type, false) || !typename_valid (descriptor.name))
        goto err_bad_param;
      dds_dynamic_type_t aliased_type = dyntype_from_typespec (gv, descriptor.base_type);
      type.ret = ddsi_dynamic_type_create_alias (gv, (struct ddsi_type **) &type.x, descriptor.name, (struct ddsi_type **) &aliased_type.x);
      break;
    }
    case DDS_DYNAMIC_ENUMERATION:
      if (!typename_valid (descriptor.name))
        goto err_bad_param;
      type.ret = ddsi_dynamic_type_create_enum (gv, (struct ddsi_type **) &type.x, descriptor.name);
      break;
    case DDS_DYNAMIC_BITMASK:
      if (!typename_valid (descriptor.name))
        goto err_bad_param;
      type.ret = ddsi_dynamic_type_create_bitmask (gv, (struct ddsi_type **) &type.x, descriptor.name);
      break;
    case DDS_DYNAMIC_ARRAY: {
      if (!typespec_valid (descriptor.element_type, false) || !typename_valid (descriptor.name) || descriptor.num_bounds == 0 || descriptor.bounds == NULL)
        goto err_bad_param;
      for (uint32_t n = 0; n < descriptor.num_bounds; n++)
        if (descriptor.bounds[n] == 0)
          goto err_bad_param;
      dds_dynamic_type_t element_type = dyntype_from_typespec (gv, descriptor.element_type);
      type.ret = ddsi_dynamic_type_create_array (gv, (struct ddsi_type **) &type.x, descriptor.name, (struct ddsi_type **) &element_type.x, descriptor.num_bounds, descriptor.bounds);
      break;
    }
    case DDS_DYNAMIC_SEQUENCE: {
      if (!typespec_valid (descriptor.element_type, false) || !typename_valid (descriptor.name) || descriptor.num_bounds > 1 || (descriptor.num_bounds == 1 && descriptor.bounds == NULL))
        goto err_bad_param;
      dds_dynamic_type_t element_type = dyntype_from_typespec (gv, descriptor.element_type);
      type.ret = ddsi_dynamic_type_create_sequence (gv, (struct ddsi_type **) &type.x, descriptor.name, (struct ddsi_type **) &element_type.x, descriptor.num_bounds > 0 ? descriptor.bounds[0] : 0);
      break;
    }
    case DDS_DYNAMIC_STRUCTURE: {
      if (!typespec_valid (descriptor.base_type, true) || !typename_valid (descriptor.name))
        goto err_bad_param;
      dds_dynamic_type_t base_type = dyntype_from_typespec (gv, descriptor.base_type);
      type.ret = ddsi_dynamic_type_create_struct (gv, (struct ddsi_type **) &type.x, descriptor.name, (struct ddsi_type **) &base_type.x);
      break;
    }
    case DDS_DYNAMIC_UNION: {
      if (!typespec_valid (descriptor.discriminator_type, false) || !union_disc_valid (descriptor.discriminator_type) || !typename_valid (descriptor.name))
        goto err_bad_param;
      dds_dynamic_type_t discriminator_type = dyntype_from_typespec (gv, descriptor.discriminator_type);
      type.ret = ddsi_dynamic_type_create_union (gv, (struct ddsi_type **) &type.x, descriptor.name, (struct ddsi_type **) &discriminator_type.x);
      break;
    }

    case DDS_DYNAMIC_CHAR16:
    case DDS_DYNAMIC_STRING16:
    case DDS_DYNAMIC_MAP:
    case DDS_DYNAMIC_BITSET:
      type.ret = DDS_RETCODE_UNSUPPORTED;
      break;
  }
  return type;

err_bad_param:
  type.ret = DDS_RETCODE_BAD_PARAMETER;
err:
  return type;
}

static dds_return_t check_type_param (const dds_dynamic_type_t *type, bool allow_non_constructing)
{
  if (type == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  if (type->ret != DDS_RETCODE_OK)
    return type->ret;
  if (!allow_non_constructing && !ddsi_dynamic_type_is_constructing ((struct ddsi_type *) type->x))
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  return DDS_RETCODE_OK;
}

dds_return_t dds_dynamic_type_add_enum_literal (dds_dynamic_type_t *type, const char *name, dds_dynamic_enum_literal_value_t value, bool is_default)
{
  dds_return_t ret;
  if ((ret = check_type_param (type, false)) != DDS_RETCODE_OK)
    return ret;
  else if (!membername_valid (name))
    type->ret = DDS_RETCODE_BAD_PARAMETER;
  else
  {
    type->ret = ddsi_dynamic_type_add_enum_literal ((struct ddsi_type *) type->x, (struct ddsi_dynamic_type_enum_literal_param) {
      .name = name,
      .is_auto_value = value.value_kind == DDS_DYNAMIC_ENUM_LITERAL_VALUE_NEXT_AVAIL,
      .value = value.value,
      .is_default = is_default
    });
  }
  return type->ret;
}

dds_return_t dds_dynamic_type_add_bitmask_field (dds_dynamic_type_t *type, const char *name, uint16_t position)
{
  dds_return_t ret;
  if ((ret = check_type_param (type, false)) != DDS_RETCODE_OK)
    return ret;
  else if (!membername_valid (name))
    type->ret = DDS_RETCODE_BAD_PARAMETER;
  else
  {
    type->ret = ddsi_dynamic_type_add_bitmask_field ((struct ddsi_type *) type->x, (struct ddsi_dynamic_type_bitmask_field_param) {
      .name = name,
      .is_auto_position = (position == DDS_DYNAMIC_BITMASK_POSITION_AUTO),
      .position = (position == DDS_DYNAMIC_BITMASK_POSITION_AUTO) ? 0 : position
    });
  }
  return type->ret;
}

dds_return_t dds_dynamic_type_add_member (dds_dynamic_type_t *type, dds_dynamic_member_descriptor_t member_descriptor)
{
  dds_return_t ret;
  if ((ret = check_type_param (type, false)) != DDS_RETCODE_OK)
    return ret;

  if (!membername_valid (member_descriptor.name))
  {
    type->ret = DDS_RETCODE_BAD_PARAMETER;
    goto err;
  }

  switch (xtkind_to_typekind (ddsi_type_get_kind ((struct ddsi_type *) type->x)))
  {
    case DDS_DYNAMIC_ENUMERATION:
      type->ret = dds_dynamic_type_add_enum_literal (type, member_descriptor.name, DDS_DYNAMIC_ENUM_LITERAL_VALUE_AUTO, member_descriptor.default_label);
      break;
    case DDS_DYNAMIC_BITMASK:
      type->ret = dds_dynamic_type_add_bitmask_field (type, member_descriptor.name, DDS_DYNAMIC_BITMASK_POSITION_AUTO);
      break;
    case DDS_DYNAMIC_UNION: {
      if (!typespec_valid (member_descriptor.type, false) ||
        (!member_descriptor.default_label && (member_descriptor.num_labels == 0 || member_descriptor.labels == NULL)))
      {
        type->ret = DDS_RETCODE_BAD_PARAMETER;
        goto err;
      }
      dds_dynamic_type_t member_type = dyntype_from_typespec (ddsi_type_get_gv ((struct ddsi_type *) type->x), member_descriptor.type);
      type->ret = ddsi_dynamic_type_add_union_member ((struct ddsi_type *) type->x, (struct ddsi_type **) &member_type.x,
          (struct ddsi_dynamic_type_union_member_param) {
            .id = member_descriptor.id,
            .name = member_descriptor.name,
            .index = member_descriptor.index,
            .is_default = member_descriptor.default_label,
            .labels = member_descriptor.labels,
            .n_labels = member_descriptor.num_labels
          });
      if (type->ret != DDS_RETCODE_OK)
        dds_dynamic_type_unref (&member_type);
      break;
    }
    case DDS_DYNAMIC_STRUCTURE: {
      if (!typespec_valid (member_descriptor.type, false))
      {
        type->ret = DDS_RETCODE_BAD_PARAMETER;
        goto err;
      }
      dds_dynamic_type_t member_type = dyntype_from_typespec (ddsi_type_get_gv ((struct ddsi_type *) type->x), member_descriptor.type);
      type->ret = ddsi_dynamic_type_add_struct_member ((struct ddsi_type *) type->x, (struct ddsi_type **) &member_type.x,
          (struct ddsi_dynamic_type_struct_member_param) {
            .id = member_descriptor.id,
            .name = member_descriptor.name,
            .index = member_descriptor.index,
            .is_key = false
          });
      if (type->ret != DDS_RETCODE_OK)
        dds_dynamic_type_unref (&member_type);
      break;
    }
    default:
      type->ret = DDS_RETCODE_BAD_PARAMETER;
      break;
  }

err:
  return type->ret;
}

dds_return_t dds_dynamic_type_set_extensibility (dds_dynamic_type_t *type, enum dds_dynamic_type_extensibility extensibility)
{
  dds_return_t ret;
  if ((ret = check_type_param (type, false)) != DDS_RETCODE_OK)
    return ret;

  if (extensibility > DDS_DYNAMIC_TYPE_EXT_MUTABLE)
    return DDS_RETCODE_BAD_PARAMETER;

  switch (xtkind_to_typekind (ddsi_type_get_kind ((struct ddsi_type *) type->x)))
  {
    case DDS_DYNAMIC_STRUCTURE:
    case DDS_DYNAMIC_UNION:
    case DDS_DYNAMIC_ENUMERATION:
    case DDS_DYNAMIC_BITMASK:
      type->ret = ddsi_dynamic_type_set_extensibility ((struct ddsi_type *) type->x, extensibility);
      break;
    default:
      type->ret = DDS_RETCODE_BAD_PARAMETER;
      break;
  }

  return type->ret;
}

dds_return_t dds_dynamic_type_set_nested (dds_dynamic_type_t *type, bool is_nested)
{
  dds_return_t ret;
  if ((ret = check_type_param (type, false)) != DDS_RETCODE_OK)
    return ret;

  switch (xtkind_to_typekind (ddsi_type_get_kind ((struct ddsi_type *) type->x)))
  {
    case DDS_DYNAMIC_STRUCTURE:
    case DDS_DYNAMIC_UNION:
      type->ret = ddsi_dynamic_type_set_nested ((struct ddsi_type *) type->x, is_nested);
      break;
    default:
      type->ret = DDS_RETCODE_BAD_PARAMETER;
      break;
  }

  return type->ret;
}

dds_return_t dds_dynamic_type_set_autoid (dds_dynamic_type_t *type, enum dds_dynamic_type_autoid value)
{
  dds_return_t ret;
  if ((ret = check_type_param (type, false)) != DDS_RETCODE_OK)
    return ret;

  if (value != DDS_DYNAMIC_TYPE_AUTOID_HASH && value != DDS_DYNAMIC_TYPE_AUTOID_SEQUENTIAL)
    type->ret = DDS_RETCODE_BAD_PARAMETER;
  else
  {
    switch (xtkind_to_typekind (ddsi_type_get_kind ((struct ddsi_type *) type->x)))
    {
      case DDS_DYNAMIC_STRUCTURE:
      case DDS_DYNAMIC_UNION:
        type->ret = ddsi_dynamic_type_set_autoid ((struct ddsi_type *) type->x, value);
        break;
      default:
        type->ret = DDS_RETCODE_BAD_PARAMETER;
        break;
    }
  }
  return type->ret;
}

dds_return_t dds_dynamic_type_set_bit_bound (dds_dynamic_type_t *type, uint16_t bit_bound)
{
  dds_return_t ret;
  if ((ret = check_type_param (type, false)) != DDS_RETCODE_OK)
    return ret;

  switch (xtkind_to_typekind (ddsi_type_get_kind ((struct ddsi_type *) type->x)))
  {
    case DDS_DYNAMIC_ENUMERATION:
      type->ret = (bit_bound > 0 && bit_bound <= 32) ? ddsi_dynamic_type_set_bitbound ((struct ddsi_type *) type->x, bit_bound) : DDS_RETCODE_BAD_PARAMETER;
      break;
    case DDS_DYNAMIC_BITMASK:
      type->ret = (bit_bound > 0 && bit_bound <= 64) ? ddsi_dynamic_type_set_bitbound ((struct ddsi_type *) type->x, bit_bound) : DDS_RETCODE_BAD_PARAMETER;
      break;
    default:
      type->ret = DDS_RETCODE_BAD_PARAMETER;
      break;
  }
  return type->ret;
}

typedef dds_return_t (*set_struct_prop_fn) (struct ddsi_type *type, uint32_t member_id, bool is_key);

static dds_return_t set_member_bool_prop (dds_dynamic_type_t *type, uint32_t member_id, bool value, set_struct_prop_fn set_fn_struct, set_struct_prop_fn set_fn_union)
{
  dds_return_t ret;
  if ((ret = check_type_param (type, false)) != DDS_RETCODE_OK)
    return ret;

  switch (xtkind_to_typekind (ddsi_type_get_kind ((struct ddsi_type *) type->x)))
  {
    case DDS_DYNAMIC_STRUCTURE:
      type->ret = set_fn_struct ? set_fn_struct ((struct ddsi_type *) type->x, member_id, value) : DDS_RETCODE_BAD_PARAMETER;
      break;
    case DDS_DYNAMIC_UNION:
      type->ret = set_fn_union ? set_fn_union ((struct ddsi_type *) type->x, member_id, value) : DDS_RETCODE_BAD_PARAMETER;
      break;
    default:
      type->ret = DDS_RETCODE_BAD_PARAMETER;
      break;
  }
  return type->ret;
}

dds_return_t dds_dynamic_member_set_key (dds_dynamic_type_t *type, uint32_t member_id, bool is_key)
{
  return (type->ret = set_member_bool_prop (type, member_id, is_key, ddsi_dynamic_type_member_set_key, 0));
}

dds_return_t dds_dynamic_member_set_optional (dds_dynamic_type_t *type, uint32_t member_id, bool is_optional)
{
  return (type->ret = set_member_bool_prop (type, member_id, is_optional, ddsi_dynamic_type_member_set_optional, 0));
}

dds_return_t dds_dynamic_member_set_external (dds_dynamic_type_t *type, uint32_t member_id, bool is_external)
{
  return (type->ret = set_member_bool_prop (type, member_id, is_external, ddsi_dynamic_struct_member_set_external, ddsi_dynamic_union_member_set_external));
}

dds_return_t dds_dynamic_member_set_hashid (dds_dynamic_type_t *type, uint32_t member_id, const char *hash_member_name)
{
  dds_return_t ret;
  if ((ret = check_type_param (type, false)) != DDS_RETCODE_OK)
    return ret;

  switch (xtkind_to_typekind (ddsi_type_get_kind ((struct ddsi_type *) type->x)))
  {
    case DDS_DYNAMIC_STRUCTURE:
    case DDS_DYNAMIC_UNION:
      type->ret = ddsi_dynamic_type_member_set_hashid ((struct ddsi_type *) type->x, member_id, hash_member_name);
      break;
    default:
      type->ret = DDS_RETCODE_BAD_PARAMETER;
      break;
  }
  return type->ret;
}

dds_return_t dds_dynamic_member_set_must_understand (dds_dynamic_type_t *type, uint32_t member_id, bool is_must_understand)
{
  return (type->ret = set_member_bool_prop (type, member_id, is_must_understand, ddsi_dynamic_type_member_set_must_understand, 0));
}

dds_return_t dds_dynamic_type_register (dds_dynamic_type_t *type, dds_typeinfo_t **type_info)
{
  dds_return_t ret;
  if ((ret = check_type_param (type, false)) != DDS_RETCODE_OK)
    return ret;
  return ddsi_dynamic_type_register ((struct ddsi_type **) &type->x, type_info);
}

dds_dynamic_type_t dds_dynamic_type_ref (dds_dynamic_type_t *type)
{
  dds_dynamic_type_t ref = { NULL, 0 };
  if ((ref.ret = check_type_param (type, true)) != DDS_RETCODE_OK)
    return ref;
  ref.x = ddsi_dynamic_type_ref ((struct ddsi_type *) type->x);
  return ref;
}

dds_return_t dds_dynamic_type_unref (dds_dynamic_type_t *type)
{
  if (type == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  ddsi_dynamic_type_unref ((struct ddsi_type *) type->x);
  return DDS_RETCODE_OK;
}

dds_dynamic_type_t dds_dynamic_type_dup (const dds_dynamic_type_t *src)
{
  dds_dynamic_type_t dst = { NULL, 0 };
  if ((dst.ret = check_type_param (src, true)) == DDS_RETCODE_OK)
  {
    dst.x = ddsi_dynamic_type_dup ((struct ddsi_type *) src->x);
    dst.ret = src->ret;
  }
  return dst;
}
