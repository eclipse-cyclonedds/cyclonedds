/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <ctype.h>
#include <stddef.h>
#include <assert.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"

#include "dds/ddsrt/fibheap.h"

#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/q_protocol.h"
#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_xevent.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/q_ddsi_discovery.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_lease.h"
#include "dds/ddsi/q_gc.h"

/* This is absolute bottom for signed integers, where -x = x and yet x
   != 0 -- and note that it had better be 2's complement machine! */
#define TSCHED_NOT_ON_HEAP INT64_MIN

const ddsrt_fibheap_def_t lease_fhdef = DDSRT_FIBHEAPDEF_INITIALIZER (offsetof (struct lease, heapnode), compare_lease_tsched);

static void force_lease_check (struct gcreq_queue *gcreq_queue)
{
  gcreq_enqueue (gcreq_new (gcreq_queue, gcreq_free));
}

int compare_lease_tsched (const void *va, const void *vb)
{
  const struct lease *a = va;
  const struct lease *b = vb;
  return (a->tsched.v == b->tsched.v) ? 0 : (a->tsched.v < b->tsched.v) ? -1 : 1;
}

int compare_lease_tdur (const void *va, const void *vb)
{
  const struct lease *a = va;
  const struct lease *b = vb;
  return (a->tdur == b->tdur) ? 0 : (a->tdur < b->tdur) ? -1 : 1;
}

void lease_management_init (struct ddsi_domaingv *gv)
{
  ddsrt_mutex_init (&gv->leaseheap_lock);
  ddsrt_fibheap_init (&lease_fhdef, &gv->leaseheap);
}

void lease_management_term (struct ddsi_domaingv *gv)
{
  assert (ddsrt_fibheap_min (&lease_fhdef, &gv->leaseheap) == NULL);
  ddsrt_mutex_destroy (&gv->leaseheap_lock);
}

struct lease *lease_new (ddsrt_etime_t texpire, dds_duration_t tdur, struct entity_common *e)
{
  struct lease *l;
  if ((l = ddsrt_malloc (sizeof (*l))) == NULL)
    return NULL;
  EETRACE (e, "lease_new(tdur %"PRId64" guid "PGUIDFMT") @ %p\n", tdur, PGUID (e->guid), (void *) l);
  l->tdur = tdur;
  ddsrt_atomic_st64 (&l->tend, (uint64_t) texpire.v);
  l->tsched.v = TSCHED_NOT_ON_HEAP;
  l->entity = e;
  return l;
}

/**
 * Returns a clone of the provided lease. Note that this function does not use
 * locking and should therefore only be called from a context where lease 'l'
 * cannot be changed by another thread during the function call.
 */
struct lease *lease_clone (const struct lease *l)
{
  ddsrt_etime_t texp;
  dds_duration_t tdur;
  texp.v = (int64_t) ddsrt_atomic_ld64 (&l->tend);
  tdur = l->tdur;
  return lease_new (texp, tdur, l->entity);
}

void lease_register (struct lease *l) /* FIXME: make lease admin struct */
{
  struct ddsi_domaingv * const gv = l->entity->gv;
  GVTRACE ("lease_register(l %p guid "PGUIDFMT")\n", (void *) l, PGUID (l->entity->guid));
  ddsrt_mutex_lock (&gv->leaseheap_lock);
  assert (l->tsched.v == TSCHED_NOT_ON_HEAP);
  int64_t tend = (int64_t) ddsrt_atomic_ld64 (&l->tend);
  if (tend != DDS_NEVER)
  {
    l->tsched.v = tend;
    ddsrt_fibheap_insert (&lease_fhdef, &gv->leaseheap, l);
  }
  ddsrt_mutex_unlock (&gv->leaseheap_lock);

  /* check_and_handle_lease_expiration runs on GC thread and the only way to be sure that it wakes up in time is by forcing re-evaluation (strictly speaking only needed if this is the first lease to expire, but this operation is quite rare anyway) */
  force_lease_check (gv->gcreq_queue);
}

void lease_unregister (struct lease *l)
{
  struct ddsi_domaingv * const gv = l->entity->gv;
  GVTRACE ("lease_unregister(l %p guid "PGUIDFMT")\n", (void *) l, PGUID (l->entity->guid));
  ddsrt_mutex_lock (&gv->leaseheap_lock);
  if (l->tsched.v != TSCHED_NOT_ON_HEAP)
  {
    ddsrt_fibheap_delete (&lease_fhdef, &gv->leaseheap, l);
    l->tsched.v = TSCHED_NOT_ON_HEAP;
  }
  ddsrt_mutex_unlock (&gv->leaseheap_lock);

  /* see lease_register() */
  force_lease_check (gv->gcreq_queue);
}

void lease_free (struct lease *l)
{
  struct ddsi_domaingv * const gv = l->entity->gv;
  GVTRACE ("lease_free(l %p guid "PGUIDFMT")\n", (void *) l, PGUID (l->entity->guid));
  ddsrt_free (l);
}

static void trace_lease_renew (const struct lease *l, const char *tag, ddsrt_etime_t tend_new)
{
  struct ddsi_domaingv const * gv = l->entity->gv;
  if (gv->logconfig.c.mask & DDS_LC_TRACE)
  {
    int32_t tsec, tusec;
    GVTRACE (" L(%s", tag);
    if (l->entity->guid.entityid.u == NN_ENTITYID_PARTICIPANT)
      GVTRACE (":%"PRIx32, l->entity->guid.entityid.u);
    else
      GVTRACE (""PGUIDFMT"", PGUID (l->entity->guid));
    ddsrt_etime_to_sec_usec (&tsec, &tusec, tend_new);
    GVTRACE (" %"PRId32".%06"PRId32")", tsec, tusec);
  }
}

void lease_renew (struct lease *l, ddsrt_etime_t tnowE)
{
  ddsrt_etime_t tend_new = ddsrt_etime_add_duration (tnowE, l->tdur);

  /* do not touch tend if moving forward or if already expired */
  int64_t tend;
  do {
    tend = (int64_t) ddsrt_atomic_ld64 (&l->tend);
    if (tend_new.v <= tend || tnowE.v >= tend)
      return;
  } while (!ddsrt_atomic_cas64 (&l->tend, (uint64_t) tend, (uint64_t) tend_new.v));

  /* Only at this point we can assume that gv can be recovered from the entity in the
   * lease (i.e. the entity still exists). In cases where dereferencing l->entity->gv
   * is not safe (e.g. the deletion of entities), the early out in the loop above
   * will be the case because tend is set to DDS_NEVER. */
  trace_lease_renew (l, "", tend_new);
}

void lease_set_expiry (struct lease *l, ddsrt_etime_t when)
{
  struct ddsi_domaingv * const gv = l->entity->gv;
  bool trigger = false;
  assert (when.v >= 0);
  ddsrt_mutex_lock (&gv->leaseheap_lock);
  /* only possible concurrent action is to move tend into the future (renew_lease),
    all other operations occur with leaseheap_lock held */
  ddsrt_atomic_st64 (&l->tend, (uint64_t) when.v);
  if (when.v < l->tsched.v)
  {
    /* moved forward and currently scheduled (by virtue of
       TSCHED_NOT_ON_HEAP == INT64_MIN) */
    l->tsched = when;
    ddsrt_fibheap_decrease_key (&lease_fhdef, &gv->leaseheap, l);
    trace_lease_renew (l, "earlier ", when);
    trigger = true;
  }
  else if (l->tsched.v == TSCHED_NOT_ON_HEAP && when.v < DDS_NEVER)
  {
    /* not currently scheduled, with a finite new expiry time */
    l->tsched = when;
    ddsrt_fibheap_insert (&lease_fhdef, &gv->leaseheap, l);
    trace_lease_renew (l, "insert ", when);
    trigger = true;
  }
  ddsrt_mutex_unlock (&gv->leaseheap_lock);

  /* see lease_register() */
  if (trigger)
    force_lease_check (gv->gcreq_queue);
}

int64_t check_and_handle_lease_expiration (struct ddsi_domaingv *gv, ddsrt_etime_t tnowE)
{
  struct lease *l;
  int64_t delay;
  ddsrt_mutex_lock (&gv->leaseheap_lock);
  while ((l = ddsrt_fibheap_min (&lease_fhdef, &gv->leaseheap)) != NULL && l->tsched.v <= tnowE.v)
  {
    ddsi_guid_t g = l->entity->guid;
    enum entity_kind k = l->entity->kind;

    assert (l->tsched.v != TSCHED_NOT_ON_HEAP);
    ddsrt_fibheap_extract_min (&lease_fhdef, &gv->leaseheap);
    /* only possible concurrent action is to move tend into the future (renew_lease),
       all other operations occur with leaseheap_lock held */
    int64_t tend = (int64_t) ddsrt_atomic_ld64 (&l->tend);
    if (tnowE.v < tend)
    {
      if (tend == DDS_NEVER) {
        /* don't reinsert if it won't expire */
        l->tsched.v = TSCHED_NOT_ON_HEAP;
      } else {
        l->tsched.v = tend;
        ddsrt_fibheap_insert (&lease_fhdef, &gv->leaseheap, l);
      }
      continue;
    }

    GVLOGDISC ("lease expired: l %p guid "PGUIDFMT" tend %"PRId64" < now %"PRId64"\n", (void *) l, PGUID (g), tend, tnowE.v);

    /* If the proxy participant is relying on another participant for
       writing its discovery data (on the privileged participant,
       i.e., its ddsi2 instance), we can't afford to drop it while the
       privileged one is still considered live.  If we do and it was a
       temporary asymmetrical thing and the ddsi2 instance never lost
       its liveliness, we will not rediscover the endpoints of this
       participant because we will not rediscover the ddsi2
       participant.

       So IF it is dependent on another one, we renew the lease for a
       very short while if the other one is still alive.  If it is a
       real case of lost liveliness, the other one will be gone soon
       enough; if not, we should get a sign of life soon enough.

       In this case, we simply abort the current iteration of the loop
       after renewing the lease and continue with the next one.

       This trick would fail if the ddsi2 participant can lose its
       liveliness and regain it before we re-check the liveliness of
       the dependent participants, and so the interval here must
       significantly less than the pruning time for the
       deleted_participants admin.

       I guess that means there is a really good argument for the SPDP
       and SEDP writers to be per-participant! */
    if (k == EK_PROXY_PARTICIPANT)
    {
      struct proxy_participant *proxypp;
      if ((proxypp = entidx_lookup_proxy_participant_guid (gv->entity_index, &g)) != NULL &&
          entidx_lookup_proxy_participant_guid (gv->entity_index, &proxypp->privileged_pp_guid) != NULL)
      {
        GVLOGDISC ("but postponing because privileged pp "PGUIDFMT" is still live\n", PGUID (proxypp->privileged_pp_guid));
        l->tsched = ddsrt_etime_add_duration (tnowE, DDS_MSECS (200));
        ddsrt_fibheap_insert (&lease_fhdef, &gv->leaseheap, l);
        continue;
      }
    }

    l->tsched.v = TSCHED_NOT_ON_HEAP;
    ddsrt_mutex_unlock (&gv->leaseheap_lock);

    switch (k)
    {
      case EK_PROXY_PARTICIPANT:
        delete_proxy_participant_by_guid (gv, &g, ddsrt_time_wallclock(), 1);
        break;
      case EK_PROXY_WRITER:
        proxy_writer_set_notalive ((struct proxy_writer *) l->entity, true);
        break;
      case EK_WRITER:
        writer_set_notalive ((struct writer *) l->entity, true);
        break;
      case EK_PARTICIPANT:
      case EK_TOPIC:
      case EK_READER:
      case EK_PROXY_READER:
        assert (false);
        break;
    }
    ddsrt_mutex_lock (&gv->leaseheap_lock);
  }

  delay = (l == NULL) ? DDS_INFINITY : (l->tsched.v - tnowE.v);
  ddsrt_mutex_unlock (&gv->leaseheap_lock);
  return delay;
}

