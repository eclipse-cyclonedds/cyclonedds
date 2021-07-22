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
#include <stddef.h>
#include <stdlib.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsi/ddsi_lifespan.h"
#include "dds/ddsi/q_xevent.h"

static int compare_lifespan_texp (const void *va, const void *vb)
{
  const struct lifespan_fhnode *a = va;
  const struct lifespan_fhnode *b = vb;
  return (a->t_expire.v == b->t_expire.v) ? 0 : (a->t_expire.v < b->t_expire.v) ? -1 : 1;
}

const ddsrt_fibheap_def_t lifespan_fhdef = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof (struct lifespan_fhnode, heapnode), compare_lifespan_texp);

static void lifespan_rhc_node_exp (struct xevent *xev, void *varg, ddsrt_mtime_t tnow)
{
  struct lifespan_adm * const lifespan_adm = varg;
  ddsrt_mtime_t next_valid = lifespan_adm->sample_expired_cb((char *)lifespan_adm - lifespan_adm->fh_offset, tnow);
  resched_xevent_if_earlier (xev, next_valid);
}


/* Gets the sample from the fibheap in lifespan admin that was expired first. If no more
 * expired samples exist in the fibheap, the expiry time (ddsrt_mtime_t) for the next sample to
 * expire is returned. If the fibheap contains no more samples, DDSRT_MTIME_NEVER is returned */
ddsrt_mtime_t lifespan_next_expired_locked (const struct lifespan_adm *lifespan_adm, ddsrt_mtime_t tnow, void **sample)
{
  struct lifespan_fhnode *node;
  if ((node = ddsrt_fibheap_min(&lifespan_fhdef, &lifespan_adm->ls_exp_heap)) != NULL && node->t_expire.v <= tnow.v)
  {
    *sample = (char *)node - lifespan_adm->fhn_offset;
    return (ddsrt_mtime_t) { 0 };
  }
  *sample = NULL;
  return (node != NULL) ? node->t_expire : DDSRT_MTIME_NEVER;
}

void lifespan_init (const struct ddsi_domaingv *gv, struct lifespan_adm *lifespan_adm, size_t fh_offset, size_t fh_node_offset, sample_expired_cb_t sample_expired_cb)
{
  ddsrt_fibheap_init (&lifespan_fhdef, &lifespan_adm->ls_exp_heap);
  lifespan_adm->evt = qxev_callback (gv->xevents, DDSRT_MTIME_NEVER, lifespan_rhc_node_exp, lifespan_adm);
  lifespan_adm->sample_expired_cb = sample_expired_cb;
  lifespan_adm->fh_offset = fh_offset;
  lifespan_adm->fhn_offset = fh_node_offset;
}

void lifespan_fini (const struct lifespan_adm *lifespan_adm)
{
  assert (ddsrt_fibheap_min (&lifespan_fhdef, &lifespan_adm->ls_exp_heap) == NULL);
  delete_xevent_callback (lifespan_adm->evt);
}

DDS_EXPORT extern inline void lifespan_register_sample_locked (struct lifespan_adm *lifespan_adm, struct lifespan_fhnode *node);

void lifespan_register_sample_real (struct lifespan_adm *lifespan_adm, struct lifespan_fhnode *node)
{
  ddsrt_fibheap_insert(&lifespan_fhdef, &lifespan_adm->ls_exp_heap, node);
  resched_xevent_if_earlier (lifespan_adm->evt, node->t_expire);
}

DDS_EXPORT extern inline void lifespan_unregister_sample_locked (struct lifespan_adm *lifespan_adm, struct lifespan_fhnode *node);

void lifespan_unregister_sample_real (struct lifespan_adm *lifespan_adm, struct lifespan_fhnode *node)
{
  /* Updating the scheduled event with the new shortest expiry
   * is not required, because the event will be rescheduled when
   * this removed node expires. Only remove the node from the
   * lifespan heap */
  ddsrt_fibheap_delete(&lifespan_fhdef, &lifespan_adm->ls_exp_heap, node);
}
