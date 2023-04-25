// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause


#include "dds/dds.h"
#include "dds/ddsi/ddsi_rhc.h"
#include "dds/ddsc/dds_rhc.h"

DDS_EXPORT extern inline dds_return_t dds_rhc_associate (struct dds_rhc *rhc, struct dds_reader *reader, const struct ddsi_sertype *type, struct ddsi_tkmap *tkmap);
DDS_EXPORT extern inline bool dds_rhc_store (struct dds_rhc * __restrict rhc, const struct ddsi_writer_info * __restrict pwr_info, struct ddsi_serdata * __restrict sample, struct ddsi_tkmap_instance * __restrict tk);
DDS_EXPORT extern inline void dds_rhc_unregister_wr (struct dds_rhc * __restrict rhc, const struct ddsi_writer_info * __restrict pwr_info);
DDS_EXPORT extern inline void dds_rhc_relinquish_ownership (struct dds_rhc * __restrict rhc, const uint64_t wr_iid);
DDS_EXPORT extern inline void dds_rhc_set_qos (struct dds_rhc *rhc, const struct dds_qos *qos);
DDS_EXPORT extern inline void dds_rhc_free (struct dds_rhc *rhc);
DDS_EXPORT extern inline int32_t dds_rhc_read (struct dds_rhc *rhc, bool lock, void **values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t mask, dds_instance_handle_t handle, struct dds_readcond *cond);
DDS_EXPORT extern inline int32_t dds_rhc_take (struct dds_rhc *rhc, bool lock, void **values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t mask, dds_instance_handle_t handle, struct dds_readcond *cond);
DDS_EXPORT extern inline int32_t dds_rhc_readcdr (struct dds_rhc *rhc, bool lock, struct ddsi_serdata **values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t sample_states, uint32_t view_states, uint32_t instance_states, dds_instance_handle_t handle);
DDS_EXPORT extern inline int32_t dds_rhc_takecdr (struct dds_rhc *rhc, bool lock, struct ddsi_serdata **values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t sample_states, uint32_t view_states, uint32_t instance_states, dds_instance_handle_t handle);
DDS_EXPORT extern inline bool dds_rhc_add_readcondition (struct dds_rhc *rhc, struct dds_readcond *cond);
DDS_EXPORT extern inline void dds_rhc_remove_readcondition (struct dds_rhc *rhc, struct dds_readcond *cond);
DDS_EXPORT extern inline uint32_t dds_rhc_lock_samples (struct dds_rhc *rhc);
