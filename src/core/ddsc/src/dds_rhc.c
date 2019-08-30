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

#include "dds/dds.h"
#include "dds/ddsi/q_rhc.h"
#include "dds__rhc.h"

extern inline bool dds_rhc_store (struct dds_rhc * __restrict rhc, const struct proxy_writer_info * __restrict pwr_info, struct ddsi_serdata * __restrict sample, struct ddsi_tkmap_instance * __restrict tk);
extern inline void dds_rhc_unregister_wr (struct dds_rhc * __restrict rhc, const struct proxy_writer_info * __restrict pwr_info);
extern inline void dds_rhc_relinquish_ownership (struct dds_rhc * __restrict rhc, const uint64_t wr_iid);
extern inline void dds_rhc_set_qos (struct dds_rhc *rhc, const struct dds_qos *qos);
extern inline void dds_rhc_free (struct dds_rhc *rhc);
extern inline int dds_rhc_read (struct dds_rhc *rhc, bool lock, void **values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t mask, dds_instance_handle_t handle, dds_readcond *cond);
extern inline int dds_rhc_take (struct dds_rhc *rhc, bool lock, void **values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t mask, dds_instance_handle_t handle, dds_readcond *cond);
extern inline int dds_rhc_takecdr (struct dds_rhc *rhc, bool lock, struct ddsi_serdata **values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t sample_states, uint32_t view_states, uint32_t instance_states, dds_instance_handle_t handle);
extern inline bool dds_rhc_add_readcondition (struct dds_readcond *cond);
extern inline void dds_rhc_remove_readcondition (struct dds_readcond *cond);
extern inline uint32_t dds_rhc_lock_samples (struct dds_rhc *rhc);
