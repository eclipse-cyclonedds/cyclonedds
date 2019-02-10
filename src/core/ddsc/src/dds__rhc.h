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
#ifndef _DDS_RHC_H_
#define _DDS_RHC_H_

#include "os/os_defs.h"

#define NO_STATE_MASK_SET   (DDS_ANY_STATE + 1)


#if defined (__cplusplus)
extern "C" {
#endif

struct rhc;
struct nn_xqos;
struct ddsi_serdata;
struct ddsi_tkmap_instance;
struct proxy_writer_info;

DDS_EXPORT struct rhc *dds_rhc_new (dds_reader *reader, const struct ddsi_sertopic *topic);
DDS_EXPORT void dds_rhc_free (struct rhc *rhc);
DDS_EXPORT void dds_rhc_fini (struct rhc *rhc);

DDS_EXPORT uint32_t dds_rhc_lock_samples (struct rhc *rhc);

DDS_EXPORT bool dds_rhc_store  (struct rhc * __restrict rhc, const struct proxy_writer_info * __restrict pwr_info, struct ddsi_serdata * __restrict sample, struct ddsi_tkmap_instance * __restrict tk);
DDS_EXPORT void dds_rhc_unregister_wr (struct rhc * __restrict rhc, const struct proxy_writer_info * __restrict pwr_info);
DDS_EXPORT void dds_rhc_relinquish_ownership (struct rhc * __restrict rhc, const uint64_t wr_iid);

DDS_EXPORT int
dds_rhc_read(
        struct rhc *rhc,
        bool lock,
        void ** values,
        dds_sample_info_t *info_seq,
        uint32_t max_samples,
        uint32_t mask,
        dds_instance_handle_t handle,
        dds_readcond *cond);
DDS_EXPORT int
dds_rhc_take(
        struct rhc *rhc,
        bool lock,
        void ** values,
        dds_sample_info_t *info_seq,
        uint32_t max_samples,
        uint32_t mask,
        dds_instance_handle_t handle,
        dds_readcond *cond);

DDS_EXPORT void dds_rhc_set_qos (struct rhc * rhc, const struct nn_xqos * qos);

DDS_EXPORT bool dds_rhc_add_readcondition (dds_readcond * cond);
DDS_EXPORT void dds_rhc_remove_readcondition (dds_readcond * cond);

DDS_EXPORT int dds_rhc_takecdr
(
  struct rhc *rhc, bool lock, struct ddsi_serdata **values, dds_sample_info_t *info_seq,
  uint32_t max_samples, unsigned sample_states,
  unsigned view_states, unsigned instance_states,
  dds_instance_handle_t handle
);

#if defined (__cplusplus)
}
#endif
#endif
