// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__TYPELIB_H
#define DDSI__TYPELIB_H

#include "dds/features.h"

#include <stdbool.h>
#include <stdint.h>
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_typewrap.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_typelib.h"

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef DDS_HAS_TYPE_DISCOVERY

extern const ddsrt_avl_treedef_t ddsi_typelib_treedef;
extern const ddsrt_avl_treedef_t ddsi_typedeps_treedef;
extern const ddsrt_avl_treedef_t ddsi_typedeps_reverse_treedef;

struct ddsi_domaingv;
struct ddsi_sertype;
struct ddsi_type;
struct ddsi_type_pair;
struct ddsi_generic_proxy_endpoint;

enum ddsi_type_state {
  DDSI_TYPE_UNRESOLVED,
  DDSI_TYPE_REQUESTED,
  DDSI_TYPE_PARTIAL_RESOLVED,
  DDSI_TYPE_RESOLVED,
  DDSI_TYPE_INVALID,
  DDSI_TYPE_CONSTRUCTING
};

/** @component type_system */
dds_return_t ddsi_type_register_dep (struct ddsi_domaingv *gv, const ddsi_typeid_t *src_type_id, struct ddsi_type **dst_dep_type, const struct DDS_XTypes_TypeIdentifier *dep_type_id);

/** @component type_system */
void ddsi_type_ref_locked (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct ddsi_type *src);

/** @component type_system */
void ddsi_type_ref (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct ddsi_type *src);

/** @component type_system */
dds_return_t ddsi_type_ref_id_locked (struct ddsi_domaingv *gv, struct ddsi_type **type, const ddsi_typeid_t *type_id);

/** @component type_system */
dds_return_t ddsi_type_ref_proxy (struct ddsi_domaingv *gv, struct ddsi_type **type, const ddsi_typeinfo_t *type_info, ddsi_typeid_kind_t kind, const ddsi_guid_t *proxy_guid);

/** @component type_system */
dds_return_t ddsi_type_add_typeobj (struct ddsi_domaingv *gv, struct ddsi_type *type, const struct DDS_XTypes_TypeObject *type_obj);

/** @component type_system */
dds_return_t ddsi_type_get_typeinfo_ser (struct ddsi_domaingv *gv, const struct ddsi_type *type, unsigned char **data, uint32_t *sz);

/** @component type_system */
dds_return_t ddsi_type_get_typeinfo (struct ddsi_domaingv *gv, const struct ddsi_type *type, struct ddsi_typeinfo *type_info);

/** @component type_system */
dds_return_t ddsi_type_get_typemap_ser (struct ddsi_domaingv *gv, const struct ddsi_type *type, unsigned char **data, uint32_t *sz);

/** @component type_system */
void ddsi_type_unreg_proxy (struct ddsi_domaingv *gv, struct ddsi_type *type, const ddsi_guid_t *proxy_guid);

/** @component type_system */
void ddsi_type_unref_locked (struct ddsi_domaingv *gv, struct ddsi_type *type);

/** @component type_system */
bool ddsi_type_resolved_locked (struct ddsi_domaingv *gv, const struct ddsi_type *type, ddsi_type_include_deps_t resolved_kind);

/** @component type_system */
void ddsi_type_free (struct ddsi_type *type);


/** @component type_system */
bool ddsi_is_assignable_from (struct ddsi_domaingv *gv, const struct ddsi_type_pair *rd_type_pair, uint32_t rd_resolved, const struct ddsi_type_pair *wr_type_pair, uint32_t wr_resolved, const dds_type_consistency_enforcement_qospolicy_t *tce);

/** @component type_system */
const ddsi_typeid_t *ddsi_type_pair_minimal_id (const struct ddsi_type_pair *type_pair);

/** @component type_system */
const ddsi_typeid_t *ddsi_type_pair_complete_id (const struct ddsi_type_pair *type_pair);

/** @component type_system */
ddsi_typeinfo_t *ddsi_type_pair_minimal_info (struct ddsi_domaingv *gv, const struct ddsi_type_pair *type_pair);

/** @component type_system */
ddsi_typeinfo_t *ddsi_type_pair_complete_info (struct ddsi_domaingv *gv, const struct ddsi_type_pair *type_pair);

/** @component type_system */
struct ddsi_type_pair *ddsi_type_pair_init (const ddsi_typeid_t *type_id_minimal, const ddsi_typeid_t *type_id_complete);

/** @component type_system */
void ddsi_type_pair_free (struct ddsi_type_pair *type_pair);
/**
 * @brief Returns the type lookup meta object for the provided type identifier.
 * @component type_system
 *
 * @note The caller of this functions needs to have locked gv->typelib_lock
 */
struct ddsi_type * ddsi_type_lookup_locked (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id);

/**
 * @component type_system
 *
 * Gets a list of proxy endpoints that are registered for the provided type
 * or for types that (indirectly) depend on this type. The resulting set of
 * endpoints is stored in the gpe_match_upd parameter. The parameter n_match_upd
 * should contain the actual number of entries in gpe_match_upd and will
 * be updated if new entries are added.
 */
void ddsi_type_get_gpe_matches (struct ddsi_domaingv *gv, const struct ddsi_type *type, struct ddsi_generic_proxy_endpoint ***gpe_match_upd, uint32_t *n_match_upd);

#endif /* DDS_HAS_TYPE_DISCOVERY */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__TYPELIB_H */
