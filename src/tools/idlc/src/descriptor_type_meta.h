// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DESCRIPTOR_TYPE_META_H
#define DESCRIPTOR_TYPE_META_H

#include "generator.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"

struct type_meta {
  bool finalized;
  struct type_meta *admin_next;
  struct type_meta *stack_prev;
  const void *node;
  DDS_XTypes_TypeIdentifier *ti_complete;
  DDS_XTypes_TypeObject *to_complete;
  DDS_XTypes_TypeIdentifier *ti_minimal;
  DDS_XTypes_TypeObject *to_minimal;
};

struct descriptor_type_meta {
  const idl_node_t *root;
  struct type_meta *admin;
  struct type_meta *stack;
};

idl_retcode_t
get_type_hash (DDS_XTypes_EquivalenceHash hash, const DDS_XTypes_TypeObject *to);

idl_retcode_t
generate_descriptor_type_meta (
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  struct descriptor_type_meta *dtm);

void
descriptor_type_meta_fini (
  struct descriptor_type_meta *dtm);

idl_retcode_t
print_type_meta_ser (
  FILE *fp,
  const idl_pstate_t *pstate,
  const idl_node_t *node);

idl_retcode_t
generate_type_meta_ser (
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  idl_typeinfo_typemap_t *result);

#endif /* DESCRIPTOR_TYPE_META_H */
