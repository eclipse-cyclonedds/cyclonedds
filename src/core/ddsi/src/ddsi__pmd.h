/*
 * Copyright(c) 2006 to 2020 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI__PMD_H
#define DDSI__PMD_H

#include <stdint.h>
#include "dds/ddsrt/time.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "ddsi__plist_generic.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_xqos.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;
struct thread_state;
struct ddsi_guid;
struct nn_xpack;
struct ddsi_participant;
struct receiver_state;

typedef struct ddsi_participant_message_data {
  ddsi_guid_prefix_t participantGuidPrefix;
  uint32_t kind; /* really 4 octets */
  ddsi_octetseq_t value;
} ddsi_participant_message_data_t;

extern const enum ddsi_pserop ddsi_participant_message_data_ops[];
extern size_t ddsi_participant_message_data_nops;
extern const enum ddsi_pserop ddsi_participant_message_data_ops_key[];
extern size_t ddsi_participant_message_data_nops_key;

void ddsi_write_pmd_message (struct thread_state * const ts1, struct nn_xpack *xp, struct ddsi_participant *pp, unsigned pmd_kind);
void ddsi_handle_pmd_message (const struct receiver_state *rst, struct ddsi_serdata *sample_common);

#if defined (__cplusplus)
}
#endif
#endif /* DDSI__PMD_H */
