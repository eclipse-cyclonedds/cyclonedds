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
#ifndef NN_MISC_H
#define NN_MISC_H

#include "dds/ddsi/q_protocol.h"

#if defined (__cplusplus)
extern "C" {
#endif

inline seqno_t fromSN (const nn_sequence_number_t sn) {
  return ((seqno_t) sn.high << 32) | sn.low;
}

inline nn_sequence_number_t toSN (seqno_t n) {
  nn_sequence_number_t x;
  x.high = (int) (n >> 32);
  x.low = (unsigned) n;
  return x;
}

unsigned char normalize_data_datafrag_flags (const SubmessageHeader_t *smhdr);

#ifdef DDS_HAS_NETWORK_PARTITIONS
int WildcardOverlap(char * p1, char * p2);
#endif

extern const ddsi_guid_t nullguid;
DDS_EXPORT bool guid_prefix_zero (const ddsi_guid_prefix_t *a);
DDS_EXPORT int guid_prefix_eq (const ddsi_guid_prefix_t *a, const ddsi_guid_prefix_t *b);
DDS_EXPORT int guid_eq (const struct ddsi_guid *a, const struct ddsi_guid *b);
DDS_EXPORT int ddsi2_patmatch (const char *pat, const char *str);

#ifdef DDS_HAS_NETWORK_PARTITIONS
struct ddsi_config;
struct ddsi_config_partitionmapping_listelem;
struct ddsi_config_partitionmapping_listelem *find_partitionmapping (const struct ddsi_config *cfg, const char *partition, const char *topic);
int is_ignored_partition (const struct ddsi_config *cfg, const char *partition, const char *topic);
#endif
#ifdef DDS_HAS_NETWORK_CHANNELS
struct ddsi_config;
struct ddsi_config_channel_listelem;
struct ddsi_config_channel_listelem *find_channel (const struct config *cfg, nn_transport_priority_qospolicy_t transport_priority);
#endif

#if defined (__cplusplus)
}
#endif

#endif /* NN_MISC_H */
