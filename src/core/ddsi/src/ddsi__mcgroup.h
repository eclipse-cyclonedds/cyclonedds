// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__MCGROUP_H
#define DDSI__MCGROUP_H

#include "dds/ddsi/ddsi_tran.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_mcgroup_membership;

/** @component multicast_group */
struct ddsi_mcgroup_membership *ddsi_new_mcgroup_membership (void);

/** @component multicast_group */
void ddsi_free_mcgroup_membership (struct ddsi_mcgroup_membership *mship);

/** @component multicast_group */
int ddsi_join_mc (const struct ddsi_domaingv *gv, struct ddsi_mcgroup_membership *mship, struct ddsi_tran_conn * conn, const ddsi_locator_t *srcip, const ddsi_locator_t *mcip);

/** @component multicast_group */
int ddsi_leave_mc (const struct ddsi_domaingv *gv, struct ddsi_mcgroup_membership *mship, struct ddsi_tran_conn * conn, const ddsi_locator_t *srcip, const ddsi_locator_t *mcip);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__MCGROUP_H */
