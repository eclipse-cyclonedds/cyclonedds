// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_TYPELIB_H
#define DDSI_TYPELIB_H

#include "dds/features.h"

#include <stdbool.h>
#include <stdint.h>
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_typewrap.h"
#include "dds/ddsi/ddsi_sertype.h"

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef DDS_HAS_TYPE_DISCOVERY

struct ddsi_domaingv;
struct ddsi_sertype;
struct ddsi_type;

typedef enum ddsi_type_request {
  DDSI_TYPE_NO_REQUEST,
  DDSI_TYPE_SEND_REQUEST
} ddsi_type_request_t;

/* Used for converting type-id to strings */
struct ddsi_typeid_str {
  char str[50];
};


/** @component type_system */
DDS_EXPORT bool ddsi_typeinfo_equal (const ddsi_typeinfo_t *a, const ddsi_typeinfo_t *b, ddsi_type_include_deps_t deps);

/** @component type_system */
DDS_EXPORT ddsi_typeid_t *ddsi_typeinfo_typeid (const ddsi_typeinfo_t *type_info, ddsi_typeid_kind_t kind);

/** @component type_system */
DDS_EXPORT ddsi_typeinfo_t *ddsi_typeinfo_deser (const unsigned char *data, uint32_t sz);

/** @component type_system */
DDS_EXPORT void ddsi_typeinfo_fini (ddsi_typeinfo_t *typeinfo);

/** @component type_system */
DDS_EXPORT void ddsi_typeinfo_free (ddsi_typeinfo_t *typeinfo);

/** @component type_system */
DDS_EXPORT ddsi_typeinfo_t * ddsi_typeinfo_dup (const ddsi_typeinfo_t *src);

/** @component type_system */
DDS_EXPORT const ddsi_typeid_t *ddsi_typeinfo_minimal_typeid (const ddsi_typeinfo_t *typeinfo);

/** @component type_system */
DDS_EXPORT const ddsi_typeid_t *ddsi_typeinfo_complete_typeid (const ddsi_typeinfo_t *typeinfo);

/** @component type_system */
DDS_EXPORT char *ddsi_make_typeid_str (struct ddsi_typeid_str *buf, const ddsi_typeid_t *type_id);

/** @component type_system */
char *ddsi_make_typeid_str_impl (struct ddsi_typeid_str *buf, const DDS_XTypes_TypeIdentifier *type_id);

/** @component type_system */
bool ddsi_typeinfo_present (const ddsi_typeinfo_t *typeinfo);

/** @component type_system */
bool ddsi_typeinfo_valid (const ddsi_typeinfo_t *typeinfo);

/** @component type_system */
DDS_EXPORT ddsi_typemap_t *ddsi_typemap_deser (const unsigned char *data, uint32_t sz);

/** @component type_system */
DDS_EXPORT void ddsi_typemap_fini (ddsi_typemap_t *typemap);

/** @component type_system */
DDS_EXPORT bool ddsi_typemap_equal (const ddsi_typemap_t *a, const ddsi_typemap_t *b);

/** @component type_system */
dds_return_t ddsi_type_ref_local (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct ddsi_sertype *sertype, ddsi_typeid_kind_t kind);

/** @component type_system */
void ddsi_type_unref (struct ddsi_domaingv *gv, struct ddsi_type *type);

/** @component type_system */
void ddsi_type_unref_sertype (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype);

/** @component type_system */
struct ddsi_typeobj *ddsi_type_get_typeobj (struct ddsi_domaingv *gv, const struct ddsi_type *type);

/** @component type_system */
bool ddsi_type_resolved (struct ddsi_domaingv *gv, const struct ddsi_type *type, ddsi_type_include_deps_t resolved_kind);

/** @component type_system */
struct ddsi_domaingv *ddsi_type_get_gv (const struct ddsi_type *type);

/** @component type_system */
DDS_XTypes_TypeKind ddsi_type_get_kind (const struct ddsi_type *type);

/**
 * @brief Waits for the provided type to be resolved
 * @component type_system
 *
 * In case the type is succesfully resolved (or was already resolved), this
 * function increases the refcount for this type. Caller should do the unref.
 */
dds_return_t ddsi_wait_for_type_resolved (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id, dds_duration_t timeout, struct ddsi_type **type, ddsi_type_include_deps_t resolved_kind, ddsi_type_request_t request);

/**
 * @brief Returns the type lookup meta object for the provided type identifier.
 * @component type_system
 *
 * @remark The returned object from this function is not refcounted,
 *   its lifetime is at lease the lifetime of the (proxy) endpoints
 *   that are referring to it.
 */
DDS_EXPORT struct ddsi_type * ddsi_type_lookup (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id);

/**
 * @brief Compares the provided type lookup meta objects.
 * @component type_system
 *
 */
DDS_EXPORT int ddsi_type_compare (const struct ddsi_type *a, const struct ddsi_type *b);

#endif /* DDS_HAS_TYPE_DISCOVERY */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_TYPELIB_H */
