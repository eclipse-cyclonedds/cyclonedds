// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__RHC_DEFAULT_H
#define DDS__RHC_DEFAULT_H

#include "dds/features.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct dds_rhc;
struct dds_reader;
struct ddsi_sertype;
struct ddsi_domaingv;
struct dds_rhc_default;
struct rhc_sample;

/** @component rhc */
struct dds_rhc *dds_rhc_default_new_xchecks (dds_reader *reader, struct ddsi_domaingv *gv, const struct ddsi_sertype *type, bool xchecks);

/** @component rhc */
struct dds_rhc *dds_rhc_default_new (struct dds_reader *reader, const struct ddsi_sertype *type);

#ifdef DDS_HAS_LIFESPAN
/** @component rhc */
ddsrt_mtime_t dds_rhc_default_sample_expired_cb(void *hc, ddsrt_mtime_t tnow);
#endif

#ifdef DDS_HAS_DEADLINE_MISSED
/** @component rhc */
ddsrt_mtime_t dds_rhc_default_deadline_missed_cb(void *hc, ddsrt_mtime_t tnow);
#endif

#if defined (__cplusplus)
}
#endif
#endif /* DDS__RHC_DEFAULT_H */
