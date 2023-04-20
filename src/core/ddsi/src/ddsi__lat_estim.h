// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__LAT_ESTIM_H
#define DDSI__LAT_ESTIM_H

#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_lat_estim.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** @component latency_estim */
void ddsi_lat_estim_init (struct ddsi_lat_estim *le);

/** @component latency_estim */
void ddsi_lat_estim_fini (struct ddsi_lat_estim *le);

/** @component latency_estim */
void ddsi_lat_estim_update (struct ddsi_lat_estim *le, int64_t est);

/** @component latency_estim */
double ddsi_lat_estim_current (const struct ddsi_lat_estim *le);

/** @component latency_estim */
int ddsi_lat_estim_log (uint32_t logcat, const struct ddsrt_log_cfg *logcfg, const char *tag, const struct ddsi_lat_estim *le);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__LAT_ESTIM_H */
