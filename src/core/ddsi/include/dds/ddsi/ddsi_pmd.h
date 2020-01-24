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

struct q_globals;
struct thread_state1;
struct ddsi_guid;
struct nn_xpack;
struct participant;
struct receiver_state;

void write_pmd_message_guid (struct q_globals * const gv, struct ddsi_guid *pp_guid, unsigned pmd_kind);
void write_pmd_message (struct thread_state1 * const ts1, struct nn_xpack *xp, struct participant *pp, unsigned pmd_kind);
void handle_pmd_message (const struct receiver_state *rst, nn_wctime_t timestamp, uint32_t statusinfo, const void *vdata, uint32_t len);

#if defined (__cplusplus)
}
#endif
#endif /* DDSI_PMD_H */
