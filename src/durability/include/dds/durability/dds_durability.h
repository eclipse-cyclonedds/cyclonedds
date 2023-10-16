// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS_DURABILITY_H
#define DDS_DURABILITY_H

#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsrt/retcode.h"
#include "ddsc/dds.h"
#include "dds__types.h"
#include "dds/ddsc/dds_rhc.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* Integration functions for durability plugins */
typedef int (*plugin_init)(const char *argument, void **context, struct ddsi_domaingv *gv);
typedef int (*plugin_finalize)(void *context);

dds_return_t dds_durability_init (const dds_domainid_t domain, struct ddsi_domaingv *gv);
dds_return_t dds_durability_fini (void);
void dds_durability_new_local_reader (struct dds_reader *reader, struct dds_rhc *rhc);
dds_return_t dds_durability_new_local_writer (dds_entity_t writer);
dds_return_t dds_durability_wait_for_quorum (dds_entity_t writer);

dds_return_t dds_durability_check_quorum_reached (struct dds_writer *writer);

bool dds_durability_is_terminating (void);


#if defined (__cplusplus)
}
#endif

#endif /* DDS_DURABILITY_H */
