// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__MISC_H
#define DDSI__MISC_H

#include "ddsi__protocol.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** @component misc */
inline ddsi_seqno_t ddsi_from_seqno (const ddsi_sequence_number_t sn)
{
  uint64_t sn_high = (uint32_t) sn.high;
  return (sn_high << 32) | sn.low;
}

/** @component misc */
inline bool ddsi_validating_from_seqno (const ddsi_sequence_number_t sn, ddsi_seqno_t *res)
{
  // ddsi_from_seqno does not checks whatsoever (and shouldn't because it is used quite a lot)
  // Valid sequence numbers are in [1 .. 2**63-1] union { SEQUENCE_NUMBER_UNKNOWN }
  // where SEQUENCE_NUMBER_UNKNOWN is the usual abomination: ((2**32-1) << 32)
  //
  // As far as I can tell, there are no messages where SEQUENCE_NUMBER_UNKNOWN is actually a
  // valid input (it would perhaps have been an elegant way to mark pre-emptive HEARTBEATs and
  // ACKNACKs, but convention is different).  So we reject them as invalid until we are forced
  // to do differently.  That leaves [1 .. 2**63-1]
  //
  // Since we use uint64_t, we can easily test by checking whether (s-1) is in [0 .. 2**63-1)
  const ddsi_seqno_t tmp = ddsi_from_seqno (sn);
  *res = tmp;
  return (tmp - 1) < DDSI_MAX_SEQ_NUMBER;
}

/** @component misc */
inline ddsi_sequence_number_t ddsi_to_seqno (ddsi_seqno_t n)
{
  ddsi_sequence_number_t x;
  x.high = (int32_t) (n >> 32);
  x.low = (uint32_t) n;
  return x;
}

/** @component misc */
unsigned char ddsi_normalize_data_datafrag_flags (const ddsi_rtps_submessage_header_t *smhdr);

extern const ddsi_guid_t ddsi_nullguid;

/** @component misc */
bool ddsi_guid_prefix_zero (const ddsi_guid_prefix_t *a);

/** @component misc */
int ddsi_guid_prefix_eq (const ddsi_guid_prefix_t *a, const ddsi_guid_prefix_t *b);

/** @component misc */
int ddsi_guid_eq (const struct ddsi_guid *a, const struct ddsi_guid *b);

/** @component misc */
int ddsi_patmatch (const char *pat, const char *str);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__MISC_H */
