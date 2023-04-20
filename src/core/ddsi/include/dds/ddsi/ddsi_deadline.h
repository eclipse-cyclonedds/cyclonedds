// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_DEADLINE_H
#define DDSI_DEADLINE_H

#include "dds/ddsrt/circlist.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_xevent.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef ddsrt_mtime_t (*deadline_missed_cb_t)(void *hc, ddsrt_mtime_t tnow);

struct ddsi_deadline_adm {
  struct ddsrt_circlist list;               /* linked list for deadline missed */
  struct ddsi_xevent *evt;                       /* xevent that triggers when deadline expires for an instance */
  deadline_missed_cb_t deadline_missed_cb;  /* callback for deadline missed; this cb can use ddsi_deadline_next_missed_locked to get next instance that has a missed deadline */
  size_t list_offset;                       /* offset of deadline_adm element in whc or rhc */
  size_t elem_offset;                       /* offset of deadline_elem element in whc or rhc instance */
  dds_duration_t dur;                       /* deadline duration */
};

struct deadline_elem {
  struct ddsrt_circlist_elem e;
  ddsrt_mtime_t t_deadline;
  ddsrt_mtime_t t_last_update;
  uint32_t deadlines_missed;
};

/** @component deadline_qos */
void ddsi_deadline_init (const struct ddsi_domaingv *gv, struct ddsi_deadline_adm *deadline_adm, size_t list_offset, size_t elem_offset, deadline_missed_cb_t deadline_missed_cb);

/** @component deadline_qos */
void ddsi_deadline_stop (const struct ddsi_deadline_adm *deadline_adm);

/** @component deadline_qos */
void ddsi_deadline_clear (struct ddsi_deadline_adm *deadline_adm);

/** @component deadline_qos */
void ddsi_deadline_fini (const struct ddsi_deadline_adm *deadline_adm);

/** @component deadline_qos */
uint32_t ddsi_deadline_compute_deadlines_missed (ddsrt_mtime_t tnow, const struct deadline_elem *deadline_elem, dds_duration_t deadline_dur);

/** @component deadline_qos */
ddsrt_mtime_t ddsi_deadline_next_missed_locked (struct ddsi_deadline_adm *deadline_adm, ddsrt_mtime_t tnow, void **instance);

/** @component deadline_qos */
void ddsi_deadline_register_instance_real (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem, ddsrt_mtime_t tnow);

/** @component deadline_qos */
void ddsi_deadline_reregister_instance_real (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem, ddsrt_mtime_t tprev, ddsrt_mtime_t tnow);

/** @component deadline_qos */
void ddsi_deadline_unregister_instance_real (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem);

/** @component deadline_qos */
void ddsi_deadline_renew_instance_real (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem);

/** @component deadline_qos */
inline void ddsi_deadline_register_instance_locked (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem, ddsrt_mtime_t tnow)
{
  if (deadline_adm->dur != DDS_INFINITY)
    ddsi_deadline_register_instance_real (deadline_adm, elem, tnow);
}

/** @component deadline_qos */
inline void ddsi_deadline_reregister_instance_locked (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem, ddsrt_mtime_t tnow)
{
  if (deadline_adm->dur != DDS_INFINITY)
    ddsi_deadline_reregister_instance_real (deadline_adm, elem, elem->t_last_update, tnow);
}

/** @component deadline_qos */
inline void ddsi_deadline_unregister_instance_locked (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem)
{
  if (deadline_adm->dur != DDS_INFINITY)
  {
    assert (elem->t_deadline.v != DDS_NEVER);
    ddsi_deadline_unregister_instance_real (deadline_adm, elem);
  }
}

/** @component deadline_qos */
inline void ddsi_deadline_renew_instance_locked (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem)
{
  if (deadline_adm->dur != DDS_INFINITY)
  {
    assert (elem->t_deadline.v != DDS_NEVER);
    ddsi_deadline_renew_instance_real (deadline_adm, elem);
  }
}

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_DEADLINE_H */

