/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI_PMD_H
#define DDSI_PMD_H

#include "dds/ddsi/q_time.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;
struct thread_state1;
struct ddsi_guid;
struct nn_xpack;
struct participant;
struct receiver_state;

typedef enum pmd_kind {
  PARTICIPANT_MESSAGE_DATA_KIND_UNKNOWN = 0x0u,
  PARTICIPANT_MESSAGE_DATA_KIND_AUTOMATIC_LIVELINESS_UPDATE = 0x1u,
  PARTICIPANT_MESSAGE_DATA_KIND_MANUAL_LIVELINESS_UPDATE = 0x2u,
  PARTICIPANT_MESSAGE_DATA_VENDER_SPECIFIC_KIND_FLAG = 0x8000000u,
} pmd_kind_t;

void write_pmd_message_guid (struct ddsi_domaingv * const gv, struct ddsi_guid *pp_guid, pmd_kind_t pmd_kind);
void write_pmd_message (struct thread_state1 * const ts1, struct nn_xpack *xp, struct participant *pp, pmd_kind_t pmd_kind);
void handle_pmd_message (const struct receiver_state *rst, nn_wctime_t timestamp, uint32_t statusinfo, const void *vdata, uint32_t len);

#if defined (__cplusplus)
}
#endif
#endif /* DDSI_PMD_H */
