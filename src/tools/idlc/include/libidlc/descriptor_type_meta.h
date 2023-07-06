// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDLC_DESCRIPTOR_TYPE_META_H
#define IDLC_DESCRIPTOR_TYPE_META_H

#include "idl_defs.h"
#include "libidlc/libidlc_export.h"

IDLC_EXPORT idl_retcode_t
print_type_meta_ser (
  FILE *fp,
  const idl_pstate_t *state,
  const idl_node_t *node);

IDLC_EXPORT idl_retcode_t
generate_type_meta_ser (
  const idl_pstate_t *state,
  const idl_node_t *node,
  idl_typeinfo_typemap_t *result);

#endif /* IDLC_DESCRIPTOR_TYPE_META_H */
