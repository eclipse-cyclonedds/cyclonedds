// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__SPDP_SCHEDULE_H
#define DDSI__SPDP_SCHEDULE_H

#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h" // FIXME: MAX_XMIT_CONNS

#if defined (__cplusplus)
extern "C" {
#endif

struct spdp_admin;

struct spdp_admin *ddsi_spdp_scheduler_new (struct ddsi_domaingv *gv)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

void ddsi_spdp_scheduler_delete (struct spdp_admin *adm)
  ddsrt_nonnull_all;

dds_return_t ddsi_spdp_register_participant (struct spdp_admin *adm, const struct ddsi_participant *pp)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

// Not an error if `pp` is not registered
void ddsi_spdp_unregister_participant (struct spdp_admin *adm, const struct ddsi_participant *pp)
  ddsrt_nonnull_all;

dds_return_t ddsi_spdp_ref_locator (struct spdp_admin *adm, const ddsi_xlocator_t *xloc)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

void ddsi_spdp_unref_locator (struct spdp_admin *adm, const ddsi_xlocator_t *xloc, bool on_lease_expiry)
  ddsrt_nonnull_all;

void ddsi_spdp_handle_aging_locators_xevent_cb (struct ddsi_domaingv *gv, struct ddsi_xevent *xev, struct ddsi_xpack *xp, void *varg, ddsrt_mtime_t tnow)
  ddsrt_nonnull ((1, 2, 3));

void ddsi_spdp_handle_live_locators_xevent_cb (struct ddsi_domaingv *gv, struct ddsi_xevent *xev, struct ddsi_xpack *xp, void *varg, ddsrt_mtime_t tnow)
  ddsrt_nonnull ((1, 2, 3));

void ddsi_spdp_force_republish (struct spdp_admin *adm, const struct ddsi_participant *pp)
  ddsrt_nonnull_all;

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__SPDP_SCHEDULE_H */
