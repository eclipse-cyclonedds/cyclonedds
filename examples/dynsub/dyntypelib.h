/*
 * Copyright(c) 2026 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#ifndef DYNTYPELIB_H
#define DYNTYPELIB_H

#include "dds/dds.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"

#include "domtree.h"
#include "type_cache.h"
#include "print_type.h"

struct dyntype {
  char *name;
  dds_dynamic_type_t *dtype;
  struct ddsi_typeinfo *typeinfo;
  DDS_XTypes_TypeObject *typeobj;
};

struct dyntypelib {
  dds_entity_t dp;
  bool print_types;
  enum dds_dynamic_type_extensibility default_extensibility;
  struct ppc ppc;
  struct ddsrt_hh *typelib;
  struct type_cache *typecache;
};

struct dyntypelib_error {
  char errmsg[256];
};

ddsrt_nonnull ((1, 3))
ddsrt_attribute_format_printf (3, 4)
dds_return_t dtl_set_error (struct dyntypelib_error *err, const struct elem *elem, const char *fmt, ...);

struct dyntypelib *dtl_new (dds_entity_t dp);
void dtl_set_print_types (struct dyntypelib *dtl, bool print_types);
void dtl_set_default_extensibility (struct dyntypelib *dtl, enum dds_dynamic_type_extensibility default_extensibility);
dds_return_t dtl_add_xml_type_library (struct dyntypelib *dtl, const char *xml_type_lib, struct dyntypelib_error *err);
dds_return_t dtl_add_typeid (struct dyntypelib *dtl, const dds_typeinfo_t *typeinfo, const DDS_XTypes_TypeObject **typeobj, struct dyntypelib_error *err);
struct dyntype *dtl_lookup_typename (struct dyntypelib *dtl, const char *name);

void dtl_print_sample (struct dyntypelib *dtl, bool valid_data, const void *sample, const DDS_XTypes_CompleteTypeObject *typeobj);
void *dtl_scan_sample (struct dyntypelib *dtl, const struct elem *input, const DDS_XTypes_CompleteTypeObject *typeobj, const bool ignore_unknown_members, struct dyntypelib_error *err);

void dtl_free (struct dyntypelib *dtl);

#endif
