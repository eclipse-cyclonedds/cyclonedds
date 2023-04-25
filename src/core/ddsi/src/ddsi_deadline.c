// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stddef.h>
#include <stdlib.h>
#include "dds/ddsrt/circlist.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsi/ddsi_deadline.h"
#include "ddsi__xevent.h"

struct instance_deadline_missed_cb_arg {
  struct ddsi_deadline_adm *deadline_adm;
};

static void instance_deadline_missed_cb (struct ddsi_domaingv *gv, struct ddsi_xevent *xev, struct ddsi_xpack *xp, void *varg, ddsrt_mtime_t tnow)
{
  (void) gv;
  (void) xp;
  struct instance_deadline_missed_cb_arg * const arg = varg;
  struct ddsi_deadline_adm * const deadline_adm = arg->deadline_adm;
  const ddsrt_mtime_t next = deadline_adm->deadline_missed_cb ((char *)deadline_adm - deadline_adm->list_offset, tnow);
  // Rate-limit repeated deadline miss notifications. The first deadline miss following an
  // update of the instance is always handled by ddsi_deadline_register_instance_real and
  // therefore always scheduled at the correct time.
  const ddsrt_mtime_t earliest_rate_limited = ddsrt_mtime_add_duration (tnow, DDS_MSECS (1));
  ddsi_resched_xevent_if_earlier (xev, (next.v > earliest_rate_limited.v) ? next : earliest_rate_limited);
}

uint32_t ddsi_deadline_compute_deadlines_missed (ddsrt_mtime_t tnow, const struct deadline_elem *deadline_elem, dds_duration_t deadline_dur)
{
  // deadline_elem->deadlines_missed + (tnow - deadline_elem->t_last_update) / deadline_dur
  // while handling all edge cases
  if (deadline_dur == 0)
  {
    // edge case: deadline = 0 means notifications by definition cannot keep up
    return UINT32_MAX;
  }
  else
  {
    const dds_duration_t dt = tnow.v - deadline_elem->t_last_update.v;
    if (dt < deadline_dur)
      return deadline_elem->deadlines_missed;
    else
    {
      // 0 < deadline_dur <= dt <= INT64_MAX => 0 < x <= INT64_MAX
      const int64_t x = dt / deadline_dur;
      if (x > (int64_t)UINT32_MAX || deadline_elem->deadlines_missed > UINT32_MAX - (uint32_t)x)
        return UINT32_MAX;
      else
        return deadline_elem->deadlines_missed + (uint32_t)x;
    }
  }
}

/* Gets the instance from the list in deadline admin that has the earliest missed deadline and
 * removes the instance element from the list. If no more instances with missed deadline exist
 * in the list, the deadline (ddsrt_mtime_t) for the first instance to 'expire' is returned. If
 * the list is empty, DDSRT_MTIME_NEVER is returned */
ddsrt_mtime_t ddsi_deadline_next_missed_locked (struct ddsi_deadline_adm *deadline_adm, ddsrt_mtime_t tnow, void **instance)
{
  struct deadline_elem *elem = NULL;
  if (!ddsrt_circlist_isempty (&deadline_adm->list))
  {
    struct ddsrt_circlist_elem *list_elem = ddsrt_circlist_oldest (&deadline_adm->list);
    elem = DDSRT_FROM_CIRCLIST (struct deadline_elem, e, list_elem);
    if (elem->t_deadline.v <= tnow.v || elem->deadlines_missed)
    {
      ddsrt_circlist_remove (&deadline_adm->list, &elem->e);
      if (instance != NULL)
        *instance = (char *)elem - deadline_adm->elem_offset;
      return (ddsrt_mtime_t) { 0 };
    }
  }
  if (instance != NULL)
    *instance = NULL;
  return (elem != NULL) ? elem->t_deadline : DDSRT_MTIME_NEVER;
}

void ddsi_deadline_init (const struct ddsi_domaingv *gv, struct ddsi_deadline_adm *deadline_adm, size_t list_offset, size_t elem_offset, deadline_missed_cb_t deadline_missed_cb)
{
  ddsrt_circlist_init (&deadline_adm->list);
  struct instance_deadline_missed_cb_arg arg = { .deadline_adm = deadline_adm };
  deadline_adm->evt = ddsi_qxev_callback (gv->xevents, DDSRT_MTIME_NEVER, instance_deadline_missed_cb, &arg, sizeof (arg), true);
  deadline_adm->deadline_missed_cb = deadline_missed_cb;
  deadline_adm->list_offset = list_offset;
  deadline_adm->elem_offset = elem_offset;
}

void ddsi_deadline_stop (const struct ddsi_deadline_adm *deadline_adm)
{
  ddsi_delete_xevent (deadline_adm->evt);
}

void ddsi_deadline_clear (struct ddsi_deadline_adm *deadline_adm)
{
  while ((ddsi_deadline_next_missed_locked (deadline_adm, DDSRT_MTIME_NEVER, NULL)).v == 0);
}

void ddsi_deadline_fini (const struct ddsi_deadline_adm *deadline_adm)
{
  assert (ddsrt_circlist_isempty (&deadline_adm->list));
  (void) deadline_adm;
}

extern inline void ddsi_deadline_register_instance_locked (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem, ddsrt_mtime_t tnow);
extern inline void ddsi_deadline_reregister_instance_locked (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem, ddsrt_mtime_t tnow);

void ddsi_deadline_register_instance_real (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem, ddsrt_mtime_t tnow)
{
  ddsrt_circlist_append (&deadline_adm->list, &elem->e);
  elem->deadlines_missed = 0;
  elem->t_last_update = tnow;
  elem->t_deadline = ddsrt_mtime_add_duration (elem->t_last_update, deadline_adm->dur);
  ddsi_resched_xevent_if_earlier (deadline_adm->evt, elem->t_deadline);
}

void ddsi_deadline_reregister_instance_real (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem, ddsrt_mtime_t tprev, ddsrt_mtime_t tnow)
{
  ddsrt_circlist_append (&deadline_adm->list, &elem->e);
  elem->deadlines_missed = 0;
  if (tnow.v <= tprev.v || deadline_adm->dur == 0)
    elem->t_last_update = tprev;
  else
  {
    const dds_duration_t dt_rounded_down =
      ((tnow.v - tprev.v) / deadline_adm->dur) * deadline_adm->dur;
    elem->t_last_update = ddsrt_mtime_add_duration (tprev, dt_rounded_down);
  }
  elem->t_deadline = ddsrt_mtime_add_duration (elem->t_last_update, deadline_adm->dur);
}

extern inline void ddsi_deadline_unregister_instance_locked (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem);

void ddsi_deadline_unregister_instance_real (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem)
{
  /* Updating the scheduled event with the new shortest expiry
   * is not required, because the event will be rescheduled when
   * this removed element expires. Only remove the element from the
   * deadline list */
  elem->t_deadline = DDSRT_MTIME_NEVER;
  ddsrt_circlist_remove (&deadline_adm->list, &elem->e);
}

extern inline void ddsi_deadline_renew_instance_locked (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem);

void ddsi_deadline_renew_instance_real (struct ddsi_deadline_adm *deadline_adm, struct deadline_elem *elem)
{
  /* move element to end of the list (list->latest) and update deadline
     according to current deadline duration in rhc (event with old deadline
     will still be triggered, but has no effect on this instance because in
     the callback the deadline (which will be the updated value) will be
     checked for expiry */
  ddsrt_mtime_t now = ddsrt_time_monotonic ();
  elem->deadlines_missed = ddsi_deadline_compute_deadlines_missed (now, elem, deadline_adm->dur);
  elem->t_last_update = now;
  if (elem->deadlines_missed == 0)
  {
    ddsrt_circlist_remove (&deadline_adm->list, &elem->e);
    elem->t_deadline = ddsrt_mtime_add_duration (now, deadline_adm->dur);
    ddsrt_circlist_append (&deadline_adm->list, &elem->e);
  }
}
