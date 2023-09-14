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

dds_return_t dds_durability_init2 (struct ddsi_domaingv* gv);
dds_return_t dds_durability_fini2 (void);
void dds_durability_new_local_reader (struct dds_reader *reader, struct dds_rhc *rhc);
void dds_durability_wait_for_ds (uint32_t quorum, dds_time_t timeout);

#if defined (__cplusplus)
}
#endif

#endif /* DDS_DURABILITY_H */
