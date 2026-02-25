// Copyright(c) 2022 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef PRINT_TYPE_H
#define PRINT_TYPE_H

#include "dds/dds.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"

struct ppc {
  bool bol;
  int indent;
  int lineno;
};

void ppc_init (struct ppc *ppc);
void ppc_print_ti (struct type_cache *tc, struct ppc *ppc, const DDS_XTypes_TypeIdentifier *typeid);
void ppc_print_to (struct type_cache *tc, struct ppc *ppc, const DDS_XTypes_CompleteTypeObject *typeobj);
void ppc_print_to_min (struct type_cache *tc, struct ppc *ppc, const DDS_XTypes_MinimalTypeObject *typeobj);

#endif /* PRINT_TYPE_H */
