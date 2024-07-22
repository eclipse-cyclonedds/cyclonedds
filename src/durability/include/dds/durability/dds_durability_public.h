// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS_DURABILITY_PUBLIC_H
#define DDS_DURABILITY_PUBLIC_H

#include "dds/ddsrt/dynlib.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsc/dds.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct dds_durability {
  ddsrt_dynlib_t lib_handle;
  dds_return_t (*dds_durability_init) (const dds_domainid_t domain, struct ddsi_domaingv *gv);
  dds_entity_t (*_dds_durability_fini) (void);
  uint32_t (*dds_durability_get_quorum) (void);
  dds_return_t (*dds_durability_new_local_reader) (dds_entity_t reader, struct dds_rhc *rhc);
  dds_return_t (*dds_durability_new_local_writer) (dds_entity_t writer);
  dds_return_t (*dds_durability_wait_for_quorum) (dds_entity_t writer);
  dds_return_t (*dds_durability_wait_for_historical_data) (dds_entity_t reader, dds_duration_t max_wait);
  bool (*dds_durability_is_terminating) (void);
} dds_durability_t;

void dds_durability_fini (dds_durability_t* dc);
dds_return_t dds_durability_load (dds_durability_t* dc, const struct ddsi_domaingv* gv);
void dds_durability_unload (dds_durability_t* dc);

#if defined (__cplusplus)
}
#endif

#endif /* DDS_DURABILITY_PUBLIC_H */
