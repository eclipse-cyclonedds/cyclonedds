// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_DYNAMIC_TYPE_H
#define DDSI_DYNAMIC_TYPE_H

#include "dds/export.h"
#include "dds/features.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_typewrap.h"
#include "dds/ddsi/ddsi_domaingv.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_dynamic_type_struct_member_param {
  uint32_t id;
  const char *name;
  uint32_t index;
  bool is_key;
};

struct ddsi_dynamic_type_union_member_param {
  uint32_t id;
  const char *name;
  uint32_t index;
  bool is_default;
  uint32_t n_labels;
  int32_t *labels;
};

struct ddsi_dynamic_type_enum_literal_param {
  const char *name;
  bool is_auto_value;
  int32_t value;
  bool is_default;
};

struct ddsi_dynamic_type_bitmask_field_param {
  const char *name;
  bool is_auto_position;
  uint16_t position;
};

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_create_struct (struct ddsi_domaingv *gv, struct ddsi_type **type, const char *type_name, struct ddsi_type **base_type);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_create_union (struct ddsi_domaingv *gv, struct ddsi_type **type, const char *type_name, struct ddsi_type **discriminant_type);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_create_sequence (struct ddsi_domaingv *gv, struct ddsi_type **type, const char *type_name, struct ddsi_type **element_type, uint32_t bound);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_create_array (struct ddsi_domaingv *gv, struct ddsi_type **type, const char *type_name, struct ddsi_type **element_type, uint32_t num_bounds, const uint32_t *bounds);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_create_enum (struct ddsi_domaingv *gv, struct ddsi_type **type, const char *type_name);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_create_bitmask (struct ddsi_domaingv *gv, struct ddsi_type **type, const char *type_name);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_create_alias (struct ddsi_domaingv *gv, struct ddsi_type **type, const char *type_name, struct ddsi_type **aliased_type);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_create_string8 (struct ddsi_domaingv *gv, struct ddsi_type **type, uint32_t bound);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_create_primitive (struct ddsi_domaingv *gv, struct ddsi_type **type, dds_dynamic_type_kind_t kind);


/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_set_extensibility (struct ddsi_type *type, enum dds_dynamic_type_extensibility extensibility);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_set_bitbound (struct ddsi_type *type, uint16_t bit_bound);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_set_nested (struct ddsi_type *type, bool is_nested);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_set_autoid (struct ddsi_type *type, enum dds_dynamic_type_autoid value);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_add_struct_member (struct ddsi_type *type, struct ddsi_type **member_type, struct ddsi_dynamic_type_struct_member_param params);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_add_union_member (struct ddsi_type *type, struct ddsi_type **member_type, struct ddsi_dynamic_type_union_member_param params);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_add_enum_literal (struct ddsi_type *type, struct ddsi_dynamic_type_enum_literal_param params);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_add_bitmask_field (struct ddsi_type *type, struct ddsi_dynamic_type_bitmask_field_param params);


/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_member_set_key (struct ddsi_type *type, uint32_t member_id, bool is_key);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_member_set_optional (struct ddsi_type *type, uint32_t member_id, bool is_optional);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_struct_member_set_external (struct ddsi_type *type, uint32_t member_id, bool is_external);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_union_member_set_external (struct ddsi_type *type, uint32_t member_id, bool is_external);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_member_set_must_understand (struct ddsi_type *type, uint32_t member_id, bool is_must_understand);

/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_member_set_hashid (struct ddsi_type *type, uint32_t member_id, const char *hash_member_name);


/** @component dynamic_type_support */
dds_return_t ddsi_dynamic_type_register (struct ddsi_type **type, ddsi_typeinfo_t **type_info);

/** @component dynamic_type_support */
struct ddsi_type * ddsi_dynamic_type_ref (struct ddsi_type *type);

/** @component dynamic_type_support */
void ddsi_dynamic_type_unref (struct ddsi_type *type);

/** @component dynamic_type_support */
struct ddsi_type * ddsi_dynamic_type_dup (const struct ddsi_type *src);

/** @component dynamic_type_support */
bool ddsi_dynamic_type_is_constructing (const struct ddsi_type *type);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_DYNAMIC_TYPE_H */
