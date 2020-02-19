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

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
int WildcardOverlap(char * p1, char * p2);
#endif

int ddsi2_patmatch (const char *pat, const char *str);

int64_t pseudo_random_delay (const ddsi_guid_t *x, const ddsi_guid_t *y, nn_mtime_t tnow, int64_t max_ms);

#if defined (__cplusplus)
}
#endif

#endif /* NN_MISC_H */
