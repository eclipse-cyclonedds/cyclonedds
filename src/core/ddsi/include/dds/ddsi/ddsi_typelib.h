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

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef DDS_HAS_TYPE_DISCOVERY

#define NOARG
DDSI_LIST_TYPES_TMPL(ddsi_type_proxy_guid_list, ddsi_guid_t, NOARG, 32)
#undef NOARG

extern const ddsrt_avl_treedef_t ddsi_typelib_treedef;

struct xt_type;
struct ddsi_domaingv;
struct generic_proxy_endpoint;

struct ddsi_type_dep {
  struct ddsi_type *type;
  struct ddsi_type_dep *prev;
};

enum ddsi_type_state {
  DDSI_TYPE_UNRESOLVED,
  DDSI_TYPE_REQUESTED,
  DDSI_TYPE_RESOLVED,
};

struct ddsi_type {
  struct xt_type xt;                            /* wrapper for XTypes type id/obj */
  ddsrt_avl_node_t avl_node;
  enum ddsi_type_state state;
  const struct ddsi_sertype *sertype;           /* sertype associated with the type identifier, NULL if type is unresolved or not used as a top-level type */
  seqno_t request_seqno;                        /* sequence number of the last type lookup request message */
  struct ddsi_type_proxy_guid_list proxy_guids; /* administration for proxy endpoints (not proxy topics) that are using this type */
  uint32_t refc;                                /* refcount for this record */
  struct ddsi_type_dep *deps;                   /* dependent type records */
};

/* The xt_type member must be at offset 0 so that the type identifier field
   in this type is at offset 0, and a ddsi_type can be used for hash table lookup
   without copying the type identifier in the search template */
DDSRT_STATIC_ASSERT (offsetof (struct ddsi_type, xt) == 0);


typedef struct ddsi_type_pair {
  struct ddsi_type *minimal;
  struct ddsi_type *complete;
} ddsi_type_pair_t;

bool ddsi_typeinfo_equal (const ddsi_typeinfo_t *a, const ddsi_typeinfo_t *b);
void ddsi_typeinfo_deserLE (unsigned char *buf, uint32_t sz, ddsi_typeinfo_t **typeinfo);
void ddsi_typeinfo_fini (ddsi_typeinfo_t *typeinfo);
ddsi_typeinfo_t * ddsi_typeinfo_dup (const ddsi_typeinfo_t *src);

void ddsi_typemap_deser (unsigned char *buf, uint32_t sz, ddsi_typemap_t **typemap);

struct ddsi_type * ddsi_type_ref_locked (struct ddsi_domaingv *gv, struct ddsi_type *type);
struct ddsi_type * ddsi_type_ref_id_locked (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id);
struct ddsi_type * ddsi_type_ref_local (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype, ddsi_typeid_kind_t kind);
struct ddsi_type * ddsi_type_ref_proxy (struct ddsi_domaingv *gv, const ddsi_typeinfo_t *type_info, ddsi_typeid_kind_t kind, const ddsi_guid_t *proxy_guid);

void ddsi_type_unreg_proxy (struct ddsi_domaingv *gv, struct ddsi_type *type, const ddsi_guid_t *proxy_guid);
void ddsi_type_unref (struct ddsi_domaingv *gv, struct ddsi_type *type);
void ddsi_type_unref_sertype (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype);
void ddsi_type_unref_locked (struct ddsi_domaingv *gv, struct ddsi_type *type);

/**
 * Returns the type lookup meta object for the provided type identifier.
 * The caller of this functions needs to have locked gv->typelib_lock
 *
 * @remark The returned object from this function is not refcounted,
 *   its lifetime is at lease the lifetime of the (proxy) endpoints
 *   that are referring to it.
 */
struct ddsi_type * ddsi_type_lookup_locked (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id);

/**
 * For all proxy endpoints registered with the type lookup meta object that is
 * associated with the provided type, this function references the sertype
 * for these endpoints.
 */
void ddsi_type_register_with_proxy_endpoints (struct ddsi_domaingv *gv, const struct ddsi_sertype *type);

/**
 * Gets a list of proxy endpoints that are registered for the provided type
 * and stores it in the gpe_match_upd parameter. The parameter n_match_upd
 * should contain the actual number of entries in gpe_match_upd and will
 * be updated if new entries are added. The return value is the number
 * of entries appended to the list.
 */
uint32_t ddsi_type_get_gpe_matches (struct ddsi_domaingv *gv, const struct ddsi_type *type, struct generic_proxy_endpoint ***gpe_match_upd, uint32_t *n_match_upd);

/**
 * Compares the provided type lookup meta objects.
 */
int ddsi_type_compare (const struct ddsi_type *a, const struct ddsi_type *b);

#endif /* DDS_HAS_TYPE_DISCOVERY */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_TYPELIB_H */
