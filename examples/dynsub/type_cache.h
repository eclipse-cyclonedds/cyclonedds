// Copyright(c) 2022 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef TYPE_CACHE_H
#define TYPE_CACHE_H

#include "dds/dds.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"

// Entry in type cache hash table: it not only caches type objects, it also caches alignment and size of
// the in-memory representation of any structure types.  These we need to get the alignment calculations
// correct.
struct typeinfo {
  union {
    uintptr_t key; // DDS_XTypes_CompleteTypeObject or DDS_XTypes_TypeIdentifier pointer
    uint32_t u32[sizeof (uintptr_t) / 4];
  } key;
  const DDS_XTypes_CompleteTypeObject *typeobj; // complete type object for type T
  const DDS_XTypes_TypeObject *release; // type object to release, or NULL if nothing
  size_t align; // _Alignof(T)
  size_t size; // sizeof(T)
};

struct type_hashid_map {
  DDS_XTypes_EquivalenceHash id;
  DDS_XTypes_TypeObject *typeobj; // complete type object for type T
  int lineno;
};

struct type_cache {
  struct ddsrt_hh *tc;
  struct ddsrt_hh *thm;
};

struct ppc;

struct type_cache *type_cache_new (void);
struct typeinfo *type_cache_lookup (struct type_cache *tc, struct typeinfo *templ);
void type_cache_add (struct type_cache *tc, struct typeinfo *info);
void type_hashid_map_add (struct type_cache *tc, struct type_hashid_map *info);
void type_cache_free (struct type_cache *tc);

struct type_hashid_map *lookup_hashid (struct type_cache *tc, const DDS_XTypes_EquivalenceHash hashid);
const DDS_XTypes_CompleteTypeObject *get_complete_typeobj_for_hashid (struct type_cache *tc, const DDS_XTypes_EquivalenceHash hashid);
const DDS_XTypes_MinimalTypeObject *get_minimal_typeobj_for_hashid (struct type_cache *tc, const DDS_XTypes_EquivalenceHash hashid);
void build_typecache_to (struct type_cache *tc, const DDS_XTypes_CompleteTypeObject *typeobj, size_t *align, size_t *size);
const DDS_XTypes_TypeObject *load_type_with_deps (struct type_cache *tc, dds_entity_t participant, const dds_typeinfo_t *typeinfo, struct ppc *ppc);
const DDS_XTypes_TypeObject *load_type_with_deps_min (struct type_cache *tc, dds_entity_t participant, const dds_typeinfo_t *typeinfo, struct ppc *ppc);
const DDS_XTypes_TypeObject *load_type_with_deps_impl (struct type_cache *tc, dds_entity_t participant, const DDS_XTypes_TypeInformation *xtypeinfo, struct ppc *ppc);
const DDS_XTypes_TypeObject *load_type_with_deps_min_impl (struct type_cache *tc, dds_entity_t participant, const DDS_XTypes_TypeInformation *xtypeinfo, struct ppc *ppc);

#endif /* TYPE_CACHE_H */
