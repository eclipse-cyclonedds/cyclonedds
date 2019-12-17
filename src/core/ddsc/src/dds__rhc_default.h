/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef _DDS_RHC_DEFAULT_H_
#define _DDS_RHC_DEFAULT_H_

#if defined (__cplusplus)
extern "C" {
#endif

struct dds_rhc;
struct dds_reader;
struct ddsi_sertopic;
struct q_globals;
struct dds_rhc_default;
struct rhc_sample;

DDS_EXPORT struct dds_rhc *dds_rhc_default_new_xchecks (dds_reader *reader, struct q_globals *gv, const struct ddsi_sertopic *topic, bool xchecks);
DDS_EXPORT struct dds_rhc *dds_rhc_default_new (struct dds_reader *reader, const struct ddsi_sertopic *topic);
DDS_EXPORT nn_mtime_t dds_rhc_default_sample_expired_cb(void *hc, nn_mtime_t tnow);

#if defined (__cplusplus)
}
#endif
#endif
