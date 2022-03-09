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
#ifndef DDSI_TYPELIB_H
#define DDSI_TYPELIB_H

#include "dds/features.h"

#include <stdbool.h>
#include <stdint.h>
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_list_tmpl.h"
#include "dds/ddsi/ddsi_typewrap.h"
#include "dds/ddsi/ddsi_sertype.h"

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef DDS_HAS_TYPE_DISCOVERY

extern const ddsrt_avl_treedef_t ddsi_typelib_treedef;

struct generic_proxy_endpoint;
struct ddsi_domaingv;
struct ddsi_sertype;
struct ddsi_sertype_cdr_data;
struct ddsi_type;
struct ddsi_type_pair;

enum ddsi_type_state {
  DDSI_TYPE_UNRESOLVED,
  DDSI_TYPE_REQUESTED,
  DDSI_TYPE_RESOLVED,
};

/* Used for converting type-id to strings */
struct ddsi_typeid_str {
  char str[50];
};

DDS_EXPORT char *ddsi_make_typeid_str (struct ddsi_typeid_str *buf, const ddsi_typeid_t *type_id);

DDS_EXPORT bool ddsi_typeinfo_equal (const ddsi_typeinfo_t *a, const ddsi_typeinfo_t *b);
DDS_EXPORT ddsi_typeid_t *ddsi_typeinfo_typeid (ddsi_typeinfo_t *type_info, ddsi_typeid_kind_t kind);
DDS_EXPORT ddsi_typeinfo_t *ddsi_typeinfo_deser (const struct ddsi_sertype_cdr_data *ser);
DDS_EXPORT void ddsi_typeinfo_fini (ddsi_typeinfo_t *typeinfo);
DDS_EXPORT ddsi_typeinfo_t * ddsi_typeinfo_dup (const ddsi_typeinfo_t *src);
DDS_EXPORT const ddsi_typeid_t *ddsi_typeinfo_minimal_typeid (const ddsi_typeinfo_t *typeinfo);
DDS_EXPORT const ddsi_typeid_t *ddsi_typeinfo_complete_typeid (const ddsi_typeinfo_t *typeinfo);
DDS_EXPORT bool ddsi_typeinfo_valid (const ddsi_typeinfo_t *typeinfo);
DDS_EXPORT uint32_t ddsi_typeinfo_get_dependent_typeids (const ddsi_typeinfo_t *type_info, const ddsi_typeid_t *** dep_ids, ddsi_typeid_kind_t kind);

DDS_EXPORT ddsi_typemap_t *ddsi_typemap_deser (const struct ddsi_sertype_cdr_data *ser);

DDS_EXPORT struct ddsi_type * ddsi_type_ref_locked (struct ddsi_domaingv *gv, struct ddsi_type *type);
DDS_EXPORT struct ddsi_type * ddsi_type_ref_id_locked (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id);
DDS_EXPORT struct ddsi_type * ddsi_type_ref_local (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype, ddsi_typeid_kind_t kind);
DDS_EXPORT struct ddsi_type * ddsi_type_ref_proxy (struct ddsi_domaingv *gv, const ddsi_typeinfo_t *type_info, ddsi_typeid_kind_t kind, const ddsi_guid_t *proxy_guid);
DDS_EXPORT const struct ddsi_sertype *ddsi_type_sertype (const struct ddsi_type *type);
DDS_EXPORT bool ddsi_type_has_typeobj (const struct ddsi_type *type);
DDS_EXPORT struct ddsi_typeobj *ddsi_type_get_typeobj (const struct ddsi_type *type);
DDS_EXPORT void ddsi_type_unreg_proxy (struct ddsi_domaingv *gv, struct ddsi_type *type, const ddsi_guid_t *proxy_guid);
DDS_EXPORT void ddsi_type_unref (struct ddsi_domaingv *gv, struct ddsi_type *type);
DDS_EXPORT void ddsi_type_unref_sertype (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype);
DDS_EXPORT void ddsi_type_unref_locked (struct ddsi_domaingv *gv, struct ddsi_type *type);

DDS_EXPORT bool ddsi_is_assignable_from (struct ddsi_domaingv *gv, const struct ddsi_type_pair *rd_type_pair, const struct ddsi_type_pair *wr_type_pair, const dds_type_consistency_enforcement_qospolicy_t *tce);
DDS_EXPORT const ddsi_typeid_t *ddsi_type_pair_minimal_id (const struct ddsi_type_pair *type_pair);
DDS_EXPORT const ddsi_typeid_t *ddsi_type_pair_complete_id (const struct ddsi_type_pair *type_pair);
DDS_EXPORT const struct ddsi_sertype *ddsi_type_pair_complete_sertype (const struct ddsi_type_pair *type_pair);
DDS_EXPORT struct ddsi_type_pair *ddsi_type_pair_init (const ddsi_typeid_t *type_id_minimal, const ddsi_typeid_t *type_id_complete);
DDS_EXPORT void ddsi_type_pair_free (struct ddsi_type_pair *type_pair);
DDS_EXPORT bool ddsi_type_pair_has_minimal_obj (const struct ddsi_type_pair *type_pair);
DDS_EXPORT bool ddsi_type_pair_has_complete_obj (const struct ddsi_type_pair *type_pair);


/**
 * Returns the type lookup meta object for the provided type identifier.
 * The caller of this functions needs to have locked gv->typelib_lock
 *
 * @remark The returned object from this function is not refcounted,
 *   its lifetime is at lease the lifetime of the (proxy) endpoints
 *   that are referring to it.
 */
DDS_EXPORT struct ddsi_type * ddsi_type_lookup_locked (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id);

/**
 * For all proxy endpoints registered with the type lookup meta object that is
 * associated with the provided type, this function references the sertype
 * for these endpoints.
 */
DDS_EXPORT void ddsi_type_register_with_proxy_endpoints (struct ddsi_domaingv *gv, const struct ddsi_sertype *type);

/**
 * Gets a list of proxy endpoints that are registered for the provided type
 * and stores it in the gpe_match_upd parameter. The parameter n_match_upd
 * should contain the actual number of entries in gpe_match_upd and will
 * be updated if new entries are added. The return value is the number
 * of entries appended to the list.
 */
DDS_EXPORT uint32_t ddsi_type_get_gpe_matches (struct ddsi_domaingv *gv, const struct ddsi_type *type, struct generic_proxy_endpoint ***gpe_match_upd, uint32_t *n_match_upd);

/**
 * Compares the provided type lookup meta objects.
 */
DDS_EXPORT int ddsi_type_compare (const struct ddsi_type *a, const struct ddsi_type *b);

#endif /* DDS_HAS_TYPE_DISCOVERY */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_TYPELIB_H */
