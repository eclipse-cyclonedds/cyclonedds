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
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsi/ddsi_lifespan.h"
#include "ddsi__xevent.h"

static int compare_lifespan_texp (const void *va, const void *vb)
{
  const struct ddsi_lifespan_fhnode *a = va;
  const struct ddsi_lifespan_fhnode *b = vb;
  return (a->t_expire.v == b->t_expire.v) ? 0 : (a->t_expire.v < b->t_expire.v) ? -1 : 1;
}

const ddsrt_fibheap_def_t lifespan_fhdef = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof (struct ddsi_lifespan_fhnode, heapnode), compare_lifespan_texp);

struct lifespan_rhc_node_exp_arg {
  struct ddsi_lifespan_adm *lifespan_adm;
};

static void lifespan_rhc_node_exp (struct ddsi_domaingv *gv, struct ddsi_xevent *xev, struct ddsi_xpack *xp, void *varg, ddsrt_mtime_t tnow)
{
  struct lifespan_rhc_node_exp_arg * const arg = varg;
  struct ddsi_lifespan_adm * const lifespan_adm = arg->lifespan_adm;
  (void) gv;
  (void) xp;
  ddsrt_mtime_t next_valid = lifespan_adm->sample_expired_cb((char *)lifespan_adm - lifespan_adm->fh_offset, tnow);
  ddsi_resched_xevent_if_earlier (xev, next_valid);
}


/* Gets the sample from the fibheap in lifespan admin that was expired first. If no more
 * expired samples exist in the fibheap, the expiry time (ddsrt_mtime_t) for the next sample to
 * expire is returned. If the fibheap contains no more samples, DDSRT_MTIME_NEVER is returned */
ddsrt_mtime_t ddsi_lifespan_next_expired_locked (const struct ddsi_lifespan_adm *lifespan_adm, ddsrt_mtime_t tnow, void **sample)
{
  struct ddsi_lifespan_fhnode *node;
  if ((node = ddsrt_fibheap_min(&lifespan_fhdef, &lifespan_adm->ls_exp_heap)) != NULL && node->t_expire.v <= tnow.v)
  {
    *sample = (char *)node - lifespan_adm->fhn_offset;
    return (ddsrt_mtime_t) { 0 };
  }
  *sample = NULL;
  return (node != NULL) ? node->t_expire : DDSRT_MTIME_NEVER;
}

void ddsi_lifespan_init (const struct ddsi_domaingv *gv, struct ddsi_lifespan_adm *lifespan_adm, size_t fh_offset, size_t fh_node_offset, ddsi_sample_expired_cb_t sample_expired_cb)
{
  ddsrt_fibheap_init (&lifespan_fhdef, &lifespan_adm->ls_exp_heap);
  struct lifespan_rhc_node_exp_arg arg = { .lifespan_adm = lifespan_adm };
  lifespan_adm->evt = ddsi_qxev_callback (gv->xevents, DDSRT_MTIME_NEVER, lifespan_rhc_node_exp, &arg, sizeof (arg), true);
  lifespan_adm->sample_expired_cb = sample_expired_cb;
  lifespan_adm->fh_offset = fh_offset;
  lifespan_adm->fhn_offset = fh_node_offset;
}

void ddsi_lifespan_fini (const struct ddsi_lifespan_adm *lifespan_adm)
{
  assert (ddsrt_fibheap_min (&lifespan_fhdef, &lifespan_adm->ls_exp_heap) == NULL);
  ddsi_delete_xevent (lifespan_adm->evt);
}

extern inline void ddsi_lifespan_register_sample_locked (struct ddsi_lifespan_adm *lifespan_adm, struct ddsi_lifespan_fhnode *node);

void ddsi_lifespan_register_sample_real (struct ddsi_lifespan_adm *lifespan_adm, struct ddsi_lifespan_fhnode *node)
{
  ddsrt_fibheap_insert(&lifespan_fhdef, &lifespan_adm->ls_exp_heap, node);
  ddsi_resched_xevent_if_earlier (lifespan_adm->evt, node->t_expire);
}

extern inline void ddsi_lifespan_unregister_sample_locked (struct ddsi_lifespan_adm *lifespan_adm, struct ddsi_lifespan_fhnode *node);

void ddsi_lifespan_unregister_sample_real (struct ddsi_lifespan_adm *lifespan_adm, struct ddsi_lifespan_fhnode *node)
{
  /* Updating the scheduled event with the new shortest expiry
   * is not required, because the event will be rescheduled when
   * this removed node expires. Only remove the node from the
   * lifespan heap */
  ddsrt_fibheap_delete(&lifespan_fhdef, &lifespan_adm->ls_exp_heap, node);
}
