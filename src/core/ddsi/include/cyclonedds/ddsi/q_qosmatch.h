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
#ifndef Q_QOSMATCH_H
#define Q_QOSMATCH_H

#if defined (__cplusplus)
extern "C" {
#endif

struct dds_qos;

int partitions_match_p (const struct dds_qos *a, const struct dds_qos *b);

/* perform reader/writer QoS (and topic name, type name, partition) matching;
   mask can be used to exclude some of these (including topic name and type
   name, so be careful!)

   reason will be set to the policy id of one of the mismatching QoS, or to
   DDS_INVALID_QOS_POLICY_ID if there is no mismatch or if the mismatch is
   in topic or type name (those are not really QoS and don't have a policy id) */
bool qos_match_mask_p (const dds_qos_t *rd, const dds_qos_t *wr, uint64_t mask, dds_qos_policy_id_t *reason) ddsrt_nonnull_all;
bool qos_match_p (const struct dds_qos *rd, const struct dds_qos *wr, dds_qos_policy_id_t *reason) ddsrt_nonnull ((1, 2));

#if defined (__cplusplus)
}
#endif

#endif /* Q_QOSMATCH_H */
