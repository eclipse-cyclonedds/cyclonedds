// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__TYPEWRAP_H
#define DDSI__TYPEWRAP_H

#include "dds/features.h"

#include <stdbool.h>
#include <stdint.h>
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"
#include "dds/ddsi/ddsi_typewrap.h"

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef DDS_HAS_TYPE_DISCOVERY

#define XT_FLAG_EXTENSIBILITY_MASK  0x7

#define DDS_XTypes_TRY_CONSTRUCT_INVALID 0
#define DDS_XTypes_TRY_CONSTRUCT_DISCARD DDS_XTypes_TRY_CONSTRUCT1
#define DDS_XTypes_TRY_CONSTRUCT_USE_DEFAULT DDS_XTypes_TRY_CONSTRUCT2
#define DDS_XTypes_TRY_CONSTRUCT_TRIM (DDS_XTypes_TRY_CONSTRUCT1 | DDS_XTypes_TRY_CONSTRUCT2)

struct xt_type;

/** @component xtypes_wrapper */
bool ddsi_type_id_with_deps_equal (const struct DDS_XTypes_TypeIdentifierWithDependencies *a, const struct DDS_XTypes_TypeIdentifierWithDependencies *b, ddsi_type_include_deps_t deps);

/** @component xtypes_wrapper */
const char * ddsi_typekind_descr (unsigned char disc);
void ddsi_xt_get_typeid_impl (const struct xt_type *xt, struct DDS_XTypes_TypeIdentifier *ti, ddsi_typeid_kind_t kind);


/** @component xtypes_wrapper */
dds_return_t ddsi_typeobj_get_hash_id (const struct DDS_XTypes_TypeObject *type_obj, ddsi_typeid_t *type_id);

/** @component xtypes_wrapper */
void ddsi_typeobj_get_hash_id_impl (const struct DDS_XTypes_TypeObject *type_obj, struct DDS_XTypes_TypeIdentifier *type_id);


/** @component xtypes_wrapper */
dds_return_t ddsi_xt_type_init (struct ddsi_domaingv *gv, struct xt_type *xt, const ddsi_typeid_t *ti, const ddsi_typeobj_t *to);

/** @component xtypes_wrapper */
dds_return_t ddsi_xt_type_add_typeobj (struct ddsi_domaingv *gv, struct xt_type *xt, const struct DDS_XTypes_TypeObject *to);

/** @component xtypes_wrapper */
void ddsi_xt_get_typeobject_kind_impl (const struct xt_type *xt, struct DDS_XTypes_TypeObject *to, ddsi_typeid_kind_t kind);

/** @component xtypes_wrapper */
void ddsi_xt_get_typeobject (const struct xt_type *xt, ddsi_typeobj_t *to);

/** @component xtypes_wrapper */
void ddsi_xt_type_fini (struct ddsi_domaingv *gv, struct xt_type *xt, bool include_typeid);

/** @component xtypes_wrapper */
bool ddsi_xt_is_assignable_from (struct ddsi_domaingv *gv, const struct xt_type *rd_xt, const struct xt_type *wr_xt, const dds_type_consistency_enforcement_qospolicy_t *tce);

/** @component xtypes_wrapper */
dds_return_t ddsi_xt_validate (struct ddsi_domaingv *gv, const struct xt_type *t);

/** @component xtypes_wrapper */
bool ddsi_xt_is_unresolved (const struct xt_type *t);

/** @component xtypes_wrapper */
bool ddsi_xt_is_resolved (const struct xt_type *t);

/** @component xtypes_wrapper */
void ddsi_xt_copy (struct ddsi_domaingv *gv, struct xt_type *dst, const struct xt_type *src);

/** @component xtypes_wrapper */
void ddsi_xt_get_namehash (DDS_XTypes_NameHash name_hash, const char *name);

#endif /* DDS_HAS_TYPE_DISCOVERY */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__TYPEWRAP_H */
