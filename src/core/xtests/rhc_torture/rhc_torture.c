// Copyright(c) 2019 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_proxy_endpoint.h"
#include "dds/ddsi/ddsi_gc.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_iid.h"
#include "ddsi__thread.h"
#include "ddsi__radmin.h"
#include "dds/dds.h"
#include "dds/ddsc/dds_rhc.h"
#include "dds__entity.h"
#include "dds__topic.h"
#include "dds__rhc_default.h"

#ifdef DDS_HAS_LIFESPAN
#include "dds/ddsi/ddsi_lifespan.h"
#endif

#include "RhcTypes.h"

#ifndef _MSC_VER
#define STATIC_ARRAY_DIM static
#else
#define STATIC_ARRAY_DIM
#endif

static ddsrt_prng_t prng;

static struct ddsi_sertype *mdtype;
static struct ddsi_thread_state *mainthread;
static dds_time_t tref_dds;
static uint32_t seq;

static ddsrt_mutex_t wait_gc_cycle_lock;
static ddsrt_cond_t wait_gc_cycle_cond;
static int wait_gc_cycle_trig;

/* these are used to get a sufficiently large result buffer when takeing/reading everying */
#define N_KEYVALS 27
#define MAX_HIST_DEPTH 4

static dds_sample_info_t rres_iseq[(MAX_HIST_DEPTH + 1) * N_KEYVALS];
static RhcTypes_T rres_mseq[sizeof (rres_iseq) / sizeof (rres_iseq[0])];
static void *rres_ptrs[sizeof (rres_iseq) / sizeof (rres_iseq[0])];
static int64_t last_dds_time = 0;

static char *print_tstamp (char *buf, size_t sz, dds_time_t t)
{
  dds_time_t d = t - tref_dds;
  size_t pos = 0;
  pos += (size_t) snprintf (buf + pos, sz - pos, "T");
  if (d / 1000000000 != 0)
    pos += (size_t) snprintf (buf + pos, sz - pos, "%+ds", (int) (d / 1000000000));
  if (d % 1000000000 != 0)
        (void) snprintf (buf + pos, sz - pos, "%+dns", (int) (d % 1000000000));
  return buf;
}

static int64_t dds_time_uniq (void)
{
  /* behaviour depends on time stamps being strictly monotonically increasing, but there
     is no way of knowing whether dds_time provides sufficient resolution, so fake it */
  int64_t t = dds_time ();
  if (t > last_dds_time)
    last_dds_time = t;
  else
    last_dds_time++;
  return last_dds_time;
}

static struct ddsi_serdata *mksample (int32_t keyval, unsigned statusinfo)
{
  RhcTypes_T d = { keyval, "A", (int32_t) ++seq, 0, "B" };
  struct ddsi_serdata *sd = ddsi_serdata_from_sample (mdtype, SDK_DATA, &d);
  sd->statusinfo = statusinfo;
  sd->timestamp.v = dds_time_uniq ();
  return sd;
}

static struct ddsi_serdata *mkkeysample (int32_t keyval, unsigned statusinfo)
{
  RhcTypes_T d = { keyval, "A", 0, 0, "B" };
  struct ddsi_serdata *sd = ddsi_serdata_from_sample (mdtype, SDK_KEY, &d);
  sd->statusinfo = statusinfo;
  sd->timestamp.v = dds_time_uniq ();
  return sd;
}

#if defined(DDS_HAS_LIFESPAN) || defined (DDS_HAS_DEADLINE_MISSED)
static ddsrt_mtime_t rand_texp (void)
{
  ddsrt_mtime_t ret = ddsrt_time_monotonic();
  ret.v -= DDS_MSECS(500) + (int64_t) (ddsrt_prng_random (&prng) % DDS_MSECS(1500));
  return ret;
}
#endif

#ifdef DDS_HAS_DEADLINE_MISSED
static dds_duration_t rand_deadline (void)
{
  return (dds_duration_t) (ddsrt_prng_random (&prng) % DDS_MSECS(500));
}
#endif

static uint64_t store (struct ddsi_tkmap *tkmap, struct dds_rhc *rhc, struct ddsi_proxy_writer *wr, struct ddsi_serdata *sd, bool print, bool lifespan_expiry)
{
#ifndef DDS_HAS_LIFESPAN
  DDSRT_UNUSED_ARG (lifespan_expiry);
#endif
  /* beware: unrefs sd */
  struct ddsi_tkmap_instance *tk;
  struct ddsi_writer_info pwr_info;
  /* single-domain application ... so domain won't change */
  ddsi_thread_state_awake_domain_ok (ddsi_lookup_thread_state ());
  tk = ddsi_tkmap_lookup_instance_ref (tkmap, sd);
  uint64_t iid = tk->m_iid;
  if (print)
  {
    RhcTypes_T d;
    char buf[64];
    char si_d = (sd->statusinfo & DDSI_STATUSINFO_DISPOSE) ? 'D' : '.';
    char si_u = (sd->statusinfo & DDSI_STATUSINFO_UNREGISTER) ? 'U' : '.';
    memset (&d, 0, sizeof (d));
    ddsi_serdata_to_sample (sd, &d, NULL, NULL);
    (void) print_tstamp (buf, sizeof (buf), sd->timestamp.v);
    if (sd->kind == SDK_KEY)
      printf ("STORE %c%c %16"PRIx64" %16"PRIx64" %2"PRId32" %6s %s\n", si_u, si_d, iid, wr->e.iid, d.k, "_", buf);
    else
      printf ("STORE %c%c %16"PRIx64" %16"PRIx64" %2"PRId32" %6"PRId32" %s\n", si_u, si_d, iid, wr->e.iid, d.k, d.x, buf);
    ddsi_sertype_free_sample (sd->type, &d, DDS_FREE_CONTENTS);
  }
  pwr_info.auto_dispose = wr->c.xqos->writer_data_lifecycle.autodispose_unregistered_instances;
  pwr_info.guid = wr->e.guid;
  pwr_info.iid = wr->e.iid;
  pwr_info.ownership_strength = wr->c.xqos->ownership_strength.value;
#ifdef DDS_HAS_LIFESPAN
  if (lifespan_expiry && (sd->statusinfo & (DDSI_STATUSINFO_UNREGISTER | DDSI_STATUSINFO_DISPOSE)) == 0)
    pwr_info.lifespan_exp = rand_texp();
  else
    pwr_info.lifespan_exp = DDSRT_MTIME_NEVER;
#endif
  dds_rhc_store (rhc, &pwr_info, sd, tk);
  ddsi_tkmap_instance_unref (tkmap, tk);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  ddsi_serdata_unref (sd);
  return iid;
}

static struct ddsi_proxy_writer *mkwr (bool auto_dispose)
{
  struct ddsi_proxy_writer *pwr;
  struct dds_qos *xqos;
  uint64_t wr_iid;
  pwr = ddsrt_malloc (sizeof (*pwr));
  xqos = ddsrt_malloc (sizeof (*xqos));
  wr_iid = ddsi_iid_gen ();
  memset (pwr, 0, sizeof (*pwr));
  ddsi_xqos_init_empty (xqos);
  ddsi_xqos_mergein_missing (xqos, &ddsi_default_qos_writer, ~(uint64_t)0);
  xqos->ownership_strength.value = 0;
  xqos->writer_data_lifecycle.autodispose_unregistered_instances = auto_dispose;
  pwr->e.iid = wr_iid;
  pwr->c.xqos = xqos;
  return pwr;
}

static void fwr (struct ddsi_proxy_writer *wr)
{
#if defined(__GNUC__) && (__GNUC__ >= 10)
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wanalyzer-double-free\"")
#endif
  ddsi_xqos_fini (wr->c.xqos);
  ddsrt_free (wr->c.xqos);
  ddsrt_free (wr);
#if defined(__GNUC__) && (__GNUC__ >= 10)
_Pragma("GCC diagnostic pop")
#endif
}

static struct dds_rhc *mkrhc (struct ddsi_domaingv *gv, dds_reader *rd, dds_history_kind_t hk, int32_t hdepth, dds_destination_order_kind_t dok)
{
  struct dds_rhc *rhc;
  dds_qos_t rqos;
  ddsi_xqos_init_empty (&rqos);
  rqos.present |= DDSI_QP_HISTORY | DDSI_QP_DESTINATION_ORDER;
  rqos.history.kind = hk;
  rqos.history.depth = hdepth;
  rqos.destination_order.kind = dok;
  ddsi_xqos_mergein_missing (&rqos, &ddsi_default_qos_reader, ~(uint64_t)0);
  ddsi_thread_state_awake_domain_ok (ddsi_lookup_thread_state ());
  rhc = dds_rhc_default_new_xchecks (rd, gv, mdtype, true);
  dds_rhc_set_qos(rhc, &rqos);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  ddsi_xqos_fini (&rqos);
  return rhc;
}

static void frhc (struct dds_rhc *rhc)
{
  ddsi_thread_state_awake_domain_ok (ddsi_lookup_thread_state ());
  dds_rhc_free (rhc);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
}

static char si2is (const dds_sample_info_t *si)
{
  switch (si->instance_state)
  {
    case DDS_ALIVE_INSTANCE_STATE: return 'A';
    case DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE: return 'D';
    case DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE: return 'U';
    default: return '?';
  }
}

static char si2ss (const dds_sample_info_t *si)
{
  switch (si->sample_state)
  {
    case DDS_READ_SAMPLE_STATE: return 'R';
    case DDS_NOT_READ_SAMPLE_STATE: return 'N';
    default: return '?';
  }
}

static char si2vs (const dds_sample_info_t *si)
{
  switch (si->view_state)
  {
    case DDS_NEW_VIEW_STATE: return 'N';
    case DDS_NOT_NEW_VIEW_STATE: return 'O';
    default: return '?';
  }
}

struct check {
  const char *st;
  uint64_t iid;
  uint64_t wr_iid;
  uint32_t dgen;
  uint32_t nwgen;
  int vd;
  int32_t keyval;
  int32_t seq;
};

static void docheck (int n, const dds_sample_info_t *iseq, const RhcTypes_T *mseq, const struct check *chk)
{
#ifndef NDEBUG
  int i;

  for (i = 0; i < n; i++)
  {
    assert (chk[i].st != 0);
    dds_sample_state_t sst = chk[i].st[0] == 'N' ? DDS_NOT_READ_SAMPLE_STATE : DDS_READ_SAMPLE_STATE;
    dds_view_state_t vst = chk[i].st[1] == 'O' ? DDS_NOT_NEW_VIEW_STATE : DDS_NEW_VIEW_STATE;
    dds_instance_state_t ist = chk[i].st[2] == 'A' ? DDS_ALIVE_INSTANCE_STATE : chk[i].st[2] == 'U' ? DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE : DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE;
    assert (iseq[i].sample_state == sst);
    assert (iseq[i].view_state == vst);
    assert (iseq[i].instance_state == ist);
    assert (iseq[i].instance_handle == chk[i].iid);
    assert (chk[i].wr_iid == 0 || iseq[i].publication_handle == chk[i].wr_iid);
    assert (iseq[i].disposed_generation_count == chk[i].dgen);
    assert (iseq[i].no_writers_generation_count == chk[i].nwgen);
    assert (!!iseq[i].valid_data == !!chk[i].vd);
    assert (mseq[i].k == chk[i].keyval);
    assert (!chk[i].vd || mseq[i].x == chk[i].seq);
  }

  assert (chk[i].st == 0);
#else
  (void)n; (void)iseq; (void)mseq; (void)chk;
#endif
}

static void print_seq (int n, const dds_sample_info_t *iseq, const RhcTypes_T *mseq)
{
  int i;
  printf ("INDX SVI %-16s %-16s DGEN NWRG SR GR AR KV    SEQ %s\n", "INSTHANDLE", "PUBHANDLE", "TSTAMP");
  for (i = 0; i < n; i++)
  {
    dds_sample_info_t const * const si = &iseq[i];
    RhcTypes_T const * const d = &mseq[i];
    char buf[64];
    assert(si->instance_handle);
    assert(si->publication_handle);
    printf ("[%2d] %c%c%c %16"PRIx64" %16"PRIx64" %4"PRIu32" %4"PRIu32" %2"PRIu32" %2"PRIu32" %2"PRIu32" %2"PRId32,
            i, si2ss(si), si2vs(si), si2is(si),
            si->instance_handle, si->publication_handle,
            si->disposed_generation_count, si->no_writers_generation_count,
            si->sample_rank, si->generation_rank, si->absolute_generation_rank,
            d->k);
    if (si->valid_data)
      printf (" %6"PRId32, d->x);
    else
      printf (" %6s", "_");
    printf (" %s\n", print_tstamp (buf, sizeof (buf), si->source_timestamp));
  }
}

static void rdtkcond (struct dds_rhc *rhc, dds_readcond *cond, const struct check *chk, bool print, int max, const char *opname, int32_t (*op) (struct dds_rhc *rhc, bool lock, void **values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t mask, dds_instance_handle_t handle, dds_readcond *cond), uint32_t states_seen[STATIC_ARRAY_DIM 2*2*3][2])
{
  int cnt;

  if (print)
    printf ("%s:\n", opname);

  ddsi_thread_state_awake_domain_ok (ddsi_lookup_thread_state ());
  cnt = op (rhc, true, rres_ptrs, rres_iseq, (max <= 0) ? (uint32_t) (sizeof (rres_iseq) / sizeof (rres_iseq[0])) : (uint32_t) max, cond ? NO_STATE_MASK_SET : (DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE), 0, cond);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  if (max > 0 && cnt > max) {
    printf ("%s TOO MUCH DATA (%d > %d)\n", opname, cnt, max);
    abort ();
  } else if (cnt > 0) {
    if (print) print_seq (cnt, rres_iseq, rres_mseq);
  } else if (cnt == 0) {
    if (print) printf ("(no data)\n");
  } else {
    printf ("%s ERROR %d\n", opname, cnt);
    abort ();
  }

  for (int i = 0; i < cnt; i++)
  {
    const int is = (rres_iseq[i].instance_state == DDS_ALIVE_INSTANCE_STATE) ? 2 : (rres_iseq[i].instance_state == DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE) ? 1 : 0;
    const int x = (rres_iseq[i].sample_state == DDS_NOT_READ_SAMPLE_STATE) + 2 * (rres_iseq[i].view_state == DDS_NEW_VIEW_STATE) + 4 * is;
    states_seen[x][rres_iseq[i].valid_data]++;

    /* invalid samples are expected to be zero except for the key fields */
    if (!rres_iseq[i].valid_data)
    {
      if (rres_mseq[i].x != 0 || rres_mseq[i].y != 0 || rres_mseq[i].s != NULL)
        abort ();
    }
  }

  /* all returned data must match cond */
  if (cond)
  {
    for (int i = 0; i < cnt; i++)
    {
      switch (cond->m_sample_states)
      {
        case DDS_SST_READ:
          if (rres_iseq[i].sample_state != DDS_READ_SAMPLE_STATE) abort ();
          break;
        case DDS_SST_NOT_READ:
          if (rres_iseq[i].sample_state != DDS_NOT_READ_SAMPLE_STATE) abort ();
          break;
      }
      switch (cond->m_view_states)
      {
        case DDS_VST_NEW:
          if (rres_iseq[i].view_state != DDS_NEW_VIEW_STATE) abort ();
          break;
        case DDS_VST_OLD:
          if (rres_iseq[i].view_state != DDS_NOT_NEW_VIEW_STATE) abort ();
          break;
      }
      switch (cond->m_instance_states)
      {
        case DDS_IST_ALIVE:
          if (rres_iseq[i].instance_state != DDS_ALIVE_INSTANCE_STATE) abort ();
          break;
        case DDS_IST_NOT_ALIVE_NO_WRITERS:
          if (rres_iseq[i].instance_state != DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE) abort ();
          break;
        case DDS_IST_NOT_ALIVE_DISPOSED:
          if (rres_iseq[i].instance_state != DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE) abort ();
          break;
        case DDS_IST_NOT_ALIVE_NO_WRITERS | DDS_IST_NOT_ALIVE_DISPOSED:
          if (rres_iseq[i].instance_state != DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE && rres_iseq[i].instance_state != DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
            abort ();
          break;
        case DDS_IST_ALIVE | DDS_IST_NOT_ALIVE_NO_WRITERS:
          if (rres_iseq[i].instance_state != DDS_ALIVE_INSTANCE_STATE && rres_iseq[i].instance_state != DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE)
            abort ();
          break;
        case DDS_IST_ALIVE | DDS_IST_NOT_ALIVE_DISPOSED:
          if (rres_iseq[i].instance_state != DDS_ALIVE_INSTANCE_STATE && rres_iseq[i].instance_state != DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
            abort ();
          break;
      }
      if (cond->m_query.m_filter)
      {
        /* invalid samples don't get the attributes zero'd out in the result, though the keys are guaranteed to be set; maybe I should change that and guarantee that the fields are 0 ... */
        if (!cond->m_query.m_filter (&rres_mseq[i]))
          abort ();
      }
    }
  }

  if (chk)
  {
    docheck (cnt, rres_iseq, rres_mseq, chk);
  }
}

static void rdall (struct dds_rhc *rhc, const struct check *chk, bool print, uint32_t states_seen[STATIC_ARRAY_DIM 2*2*3][2])
{
  rdtkcond (rhc, NULL, chk, print, 0, "READ ALL", dds_rhc_read, states_seen);
}

static void tkall (struct dds_rhc *rhc, const struct check *chk, bool print, uint32_t states_seen[STATIC_ARRAY_DIM 2*2*3][2])
{
  rdtkcond (rhc, NULL, chk, print, 0, "TAKE ALL", dds_rhc_take, states_seen);
}

static void print_condmask (char *buf, size_t bufsz, const dds_readcond *cond)
{
  size_t pos = 0;
  const char *sep = "";
  pos += (size_t) snprintf (buf + pos, bufsz - pos, "[");
  switch (cond->m_sample_states)
  {
    case DDS_SST_READ:
      pos += (size_t) snprintf (buf + pos, bufsz - pos, "%sREAD", sep);
      sep = ", ";
      break;
    case DDS_SST_NOT_READ:
      pos += (size_t) snprintf (buf + pos, bufsz - pos, "%sNOT_READ", sep);
      sep = ", ";
      break;
  }
  switch (cond->m_view_states)
  {
    case DDS_VST_NEW:
      pos += (size_t) snprintf (buf + pos, bufsz - pos, "%sNEW", sep);
      sep = ", ";
      break;
    case DDS_VST_OLD:
      pos += (size_t) snprintf (buf + pos, bufsz - pos, "%sOLD", sep);
      sep = ", ";
      break;
  }
  switch (cond->m_instance_states)
  {
    case DDS_IST_ALIVE:
      pos += (size_t) snprintf (buf + pos, bufsz - pos, "%sALIVE", sep);
      break;
    case DDS_IST_NOT_ALIVE_NO_WRITERS:
      pos += (size_t) snprintf (buf + pos, bufsz - pos, "%sNO_WRITERS", sep);
      break;
    case DDS_IST_NOT_ALIVE_DISPOSED:
      pos += (size_t) snprintf (buf + pos, bufsz - pos, "%sDISPOSED", sep);
      break;
    case DDS_IST_NOT_ALIVE_NO_WRITERS | DDS_IST_NOT_ALIVE_DISPOSED:
      pos += (size_t) snprintf (buf + pos, bufsz - pos, "%sNOT_ALIVE", sep);
      break;
    case DDS_IST_ALIVE | DDS_IST_NOT_ALIVE_NO_WRITERS:
      pos += (size_t) snprintf (buf + pos, bufsz - pos, "%sALIVE | NO_WRITERS", sep);
      break;
    case DDS_IST_ALIVE | DDS_IST_NOT_ALIVE_DISPOSED:
      pos += (size_t) snprintf (buf + pos, bufsz - pos, "%sALIVE | DISPOSED", sep);
      break;
  }
  (void) snprintf (buf + pos, bufsz - pos, "]");
}

static void rdcond (struct dds_rhc *rhc, dds_readcond *cond, const struct check *chk, int max, bool print, uint32_t states_seen[STATIC_ARRAY_DIM 2*2*3][2])
{
  char buf[100];
  int pos;
  pos = snprintf (buf, sizeof (buf), "READ COND %p %d ", (void *) cond, max);
  print_condmask (buf + pos, sizeof (buf) - (size_t) pos, cond);
  rdtkcond (rhc, cond, chk, print, max, buf, dds_rhc_read, states_seen);
}

static void tkcond (struct dds_rhc *rhc, dds_readcond *cond, const struct check *chk, int max, bool print, uint32_t states_seen[STATIC_ARRAY_DIM 2*2*3][2])
{
  char buf[100];
  int pos;
  pos = snprintf (buf, sizeof (buf), "TAKE COND %p %d ", (void *) cond, max);
  print_condmask (buf + pos, sizeof (buf) - (size_t) pos, cond);
  rdtkcond (rhc, cond, chk, print, max, buf, dds_rhc_take, states_seen);
}

static void wait_gc_cycle_impl (struct ddsi_gcreq *gcreq)
{
  ddsrt_mutex_lock (&wait_gc_cycle_lock);
  wait_gc_cycle_trig = 1;
  ddsrt_cond_broadcast (&wait_gc_cycle_cond);
  ddsrt_mutex_unlock (&wait_gc_cycle_lock);
  ddsi_gcreq_free (gcreq);
}

static void wait_gc_cycle (struct ddsi_gcreq_queue *gcreq_queue)
{
  /* only single-threaded for now */
  struct ddsi_gcreq *gcreq = ddsi_gcreq_new (gcreq_queue, wait_gc_cycle_impl);
#ifndef NDEBUG
  ddsrt_mutex_lock (&wait_gc_cycle_lock);
  assert (wait_gc_cycle_trig == 0);
  ddsrt_mutex_unlock (&wait_gc_cycle_lock);
#endif
  ddsi_gcreq_enqueue (gcreq);
  ddsrt_mutex_lock (&wait_gc_cycle_lock);
  while (!wait_gc_cycle_trig)
    ddsrt_cond_wait (&wait_gc_cycle_cond, &wait_gc_cycle_lock);
  wait_gc_cycle_trig = 0;
  ddsrt_mutex_unlock (&wait_gc_cycle_lock);
}

static bool qcpred_key (const void *vx)
{
  const RhcTypes_T *x = vx;
  return (x->k % 2) == 0;
}

static bool qcpred_attr2 (const void *vx)
{
  const RhcTypes_T *x = vx;
  return (x->x % 2) == 0;
}

static bool qcpred_attr3 (const void *vx)
{
  const RhcTypes_T *x = vx;
  return (x->x % 3) == 0;
}

static dds_readcond *get_condaddr (dds_entity_t x)
{
  struct dds_entity *e;
  if (dds_entity_lock (x, DDS_KIND_DONTCARE, &e) < 0)
    abort();
  assert (dds_entity_kind (e) == DDS_KIND_COND_READ || dds_entity_kind (e) == DDS_KIND_COND_QUERY);
  dds_entity_unlock (e);
  return (dds_readcond *) e;
}

static void print_cond_w_addr (const char *label, dds_entity_t x)
{
  char buf[100];
  struct dds_entity *e;
  if (dds_entity_lock (x, DDS_KIND_DONTCARE, &e) < 0)
    abort();
  assert (dds_entity_kind (e) == DDS_KIND_COND_READ || dds_entity_kind (e) == DDS_KIND_COND_QUERY);
  print_condmask (buf, sizeof (buf), (dds_readcond *) e);
  printf ("%s: %"PRId32" => %p %s\n", label, x, (void *) e, buf);
  dds_entity_unlock (e);
}

static dds_entity_t readcond_wrapper (dds_entity_t reader, uint32_t mask, dds_querycondition_filter_fn filter)
{
  (void) filter;
  return dds_create_readcondition (reader, mask);
}

static struct ddsi_domaingv *get_gv (dds_entity_t e)
{
  struct ddsi_domaingv *gv;
  dds_entity *x;
  if (dds_entity_pin (e, &x) < 0)
    abort ();
  gv = &x->m_domain->gv;
  dds_entity_unpin (x);
  return gv;
}

static void test_conditions (dds_entity_t pp, dds_entity_t tp, const int count, dds_entity_t (*create_cond) (dds_entity_t reader, uint32_t mask, dds_querycondition_filter_fn filter), dds_querycondition_filter_fn filter0, dds_querycondition_filter_fn filter1, bool print)
{
  dds_qos_t *qos = dds_create_qos ();
  dds_qset_history (qos, DDS_HISTORY_KEEP_LAST, MAX_HIST_DEPTH);
  dds_qset_destination_order (qos, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);
#ifdef DDS_HAS_DEADLINE_MISSED
  dds_qset_deadline (qos, rand_deadline());
#endif
  /* two identical readers because we need 63 conditions while we can currently only attach 32 a single reader */
  dds_entity_t rd[] = { dds_create_reader (pp, tp, qos, NULL), dds_create_reader (pp, tp, qos, NULL) };
  const size_t nrd = sizeof (rd) / sizeof (rd[0]);
  dds_delete_qos (qos);
  struct dds_rhc *rhc[sizeof (rd) / sizeof (rd[0])];
  for (size_t i = 0; i < sizeof (rd) / sizeof (rd[0]); i++)
  {
    struct dds_entity *x;
    if (dds_entity_lock (rd[i], DDS_KIND_READER, &x) < 0)
      abort ();
    dds_reader *rdp = (dds_reader *) x;
    rhc[i] = rdp->m_rhc;
    dds_entity_unlock (x);
  }

  const struct ddsi_domaingv *gv = get_gv (pp);
  struct ddsi_tkmap *tkmap = gv->m_tkmap;
  struct ddsi_proxy_writer *wr[] = { mkwr (0), mkwr (1), mkwr (1) };

  static const uint32_t stab[] = {
    DDS_READ_SAMPLE_STATE, DDS_NOT_READ_SAMPLE_STATE,
    DDS_READ_SAMPLE_STATE | DDS_NOT_READ_SAMPLE_STATE
  };
  const int nstab = (int) (sizeof (stab) / sizeof (stab[0]));
  static const uint32_t vtab[] = {
    DDS_NEW_VIEW_STATE, DDS_NOT_NEW_VIEW_STATE,
    DDS_NEW_VIEW_STATE | DDS_NOT_NEW_VIEW_STATE
  };
  const int nvtab = (int) (sizeof (vtab) / sizeof (vtab[0]));
  static const uint32_t itab[] = {
    DDS_ALIVE_INSTANCE_STATE, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE,
    DDS_ALIVE_INSTANCE_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE,
    DDS_ALIVE_INSTANCE_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE,
    DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE,
    DDS_ALIVE_INSTANCE_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE
  };
  const int nitab = (int) (sizeof (itab) / sizeof (itab[0]));
  const int nconds = nstab * nvtab * nitab;

  dds_entity_t gdcond = dds_create_guardcondition (pp);
  dds_entity_t waitset = dds_create_waitset(pp);
  dds_waitset_attach(waitset, gdcond, 888);

  /* create a conditions for every possible state mask */
  assert (nconds == 63);
  dds_entity_t conds[63];
  dds_readcond *rhcconds[63];
  {
    int ci = 0;
    for (int s = 0; s < nstab; s++)
      for (int v = 0; v < nvtab; v++)
        for (int i = 0; i < nitab; i++)
        {
          assert (ci / 32 < (int) (sizeof (rd) / sizeof (rd[0])));
          conds[ci] = create_cond (rd[ci / 32], stab[s] | vtab[v] | itab[i], ((ci % 2) == 0) ? filter0 : filter1);
          if (conds[ci] <= 0) abort ();
          rhcconds[ci] = get_condaddr (conds[ci]);
          if (print) {
            char buf[18];
            snprintf (buf, sizeof (buf), "conds[%d]", ci);
            print_cond_w_addr (buf, conds[ci]);
          }
          dds_waitset_attach(waitset, conds[ci], ci);
          ci++;
        }
  }

  /* simply sanity check on the guard condition and waitset triggering */
  {
    bool v;
    int n;
    dds_attach_t xs[2];
    dds_read_guardcondition (gdcond, &v);
    assert (!v);
    dds_set_guardcondition (gdcond, true);
    n = dds_waitset_wait (waitset, xs, sizeof(xs) / sizeof(xs[0]), 0);
    assert (n == 1);
    (void)n;
    assert (xs[0] == 888);
    dds_read_guardcondition (gdcond, &v);
    assert (v);
    dds_take_guardcondition (gdcond, &v);
    assert (v);
    dds_read_guardcondition (gdcond, &v);
    assert (!v);
    n = dds_waitset_wait (waitset, xs, sizeof(xs) / sizeof(xs[0]), 0);
    assert (n == 0);
    (void)n;
  }

  /* relative frequency table of operations: */
  static const char *operstr[] = {
    [0] = "w",
    [1] = "wd",
    [2] = "d",
    [3] = "u",
    [4] = "du",
    [5] = "wdu",
    [6] = "rdall",
    [7] = "tkall",
    [8] = "rdc",
    [9] = "tkc",
    [10] = "tkc1",
    [11] = "delwr",
    [12] = "drpxp",
    [13] = "dlmis"
  };
  static const uint32_t opfreqs[] = {
    [0]  = 500, /* write */
    [1]  = 100, /* variants with dispose & unregister */
    [2]  = 100,
    [3]  = 300, /* just unregister */
    [4]  = 100, /* variants with dispose & unregister */
    [5]  = 100,
    [6]  = 50,  /* read all */
    [7]  = 5,   /* take all */
    [8]  = 200, /* read cond */
    [9]  = 30,  /* take cond */
    [10] = 100, /* take cond, max 1 */
    [11] = 1,   /* unreg writer */
#ifdef DDS_HAS_LIFESPAN
    [12] = 100, /* drop expired sample */
#else
    [12] = 0,   /* drop expired sample */
#endif
#ifdef DDS_HAS_DEADLINE_MISSED
    [13] = 100  /* deadline missed */
#else
    [13] = 0    /* drop expired sample */
#endif
  };
  uint32_t opthres[sizeof (opfreqs) / sizeof (opfreqs[0])];
  {
    const size_t n = sizeof (opfreqs) / sizeof (opfreqs[0]);
    uint32_t sum = 0;
    for (size_t i = 0; i < n; i++)
      sum += opfreqs[i];
    const uint32_t scale = UINT32_MAX / sum;
    sum = 0;
    for (size_t i = 0; i < n; i++)
    {
      sum += opfreqs[i];
      opthres[i] = sum * scale;
    }
  }

  uint32_t states_seen[2 * 2 * 3][2] = {{ 0 }};
  uint32_t opcount[sizeof (opfreqs) / sizeof (opfreqs[0])] = { 0 };
  int lastprint_pct = 0;
  for (int i = 0; i < count; i++)
  {
    const int32_t keyval = (int32_t) (ddsrt_prng_random (&prng) % N_KEYVALS);
    const uint32_t which = ddsrt_prng_random (&prng) % 3;
    uint32_t oper_base;
    uint32_t oper;

    /* generate uniform number in range 0 .. N, then map to operation following the frequency table */
    do {
      oper_base = ddsrt_prng_random (&prng);
    } while (oper_base >= opthres[sizeof (opfreqs) / sizeof (opfreqs[0]) - 1]);
    for (oper = 0; oper < sizeof (opfreqs) / sizeof (opfreqs[0]); oper++)
    {
      if (oper_base < opthres[oper])
        break;
    }
    opcount[oper]++;

    if (100 * i / count > lastprint_pct)
    {
      lastprint_pct = 100 * i / count;
      printf ("%d%%%c", lastprint_pct, print ? '\n' : '\r');
      fflush (stdout);
    }

    switch (oper)
    {
      case 0: { /* wr */
        struct ddsi_serdata *s = mksample (keyval, 0);
        for (size_t k = 0; k < nrd; k++)
          store (tkmap, rhc[k], wr[which], ddsi_serdata_ref (s), print && k == 0, true);
        ddsi_serdata_unref (s);
        break;
      }
      case 1: { /* wr disp */
        struct ddsi_serdata *s = mksample (keyval, DDSI_STATUSINFO_DISPOSE);
        for (size_t k = 0; k < nrd; k++)
          store (tkmap, rhc[k], wr[which], ddsi_serdata_ref (s), print && k == 0, true);
        ddsi_serdata_unref (s);
        break;
      }
      case 2: { /* disp */
        struct ddsi_serdata *s = mkkeysample (keyval, DDSI_STATUSINFO_DISPOSE);
        for (size_t k = 0; k < nrd; k++)
          store (tkmap, rhc[k], wr[which], ddsi_serdata_ref (s), print && k == 0, true);
        ddsi_serdata_unref (s);
        break;
      }
      case 3: { /* unreg */
        struct ddsi_serdata *s = mkkeysample (keyval, DDSI_STATUSINFO_UNREGISTER);
        for (size_t k = 0; k < nrd; k++)
          store (tkmap, rhc[k], wr[which], ddsi_serdata_ref (s), print && k == 0, true);
        ddsi_serdata_unref (s);
        break;
      }
      case 4: { /* disp unreg */
        struct ddsi_serdata *s = mkkeysample (keyval, DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER);
        for (size_t k = 0; k < nrd; k++)
          store (tkmap, rhc[k], wr[which], ddsi_serdata_ref (s), print && k == 0, true);
        ddsi_serdata_unref (s);
        break;
      }
      case 5: { /* wr disp unreg */
        struct ddsi_serdata *s = mksample (keyval, DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER);
        for (size_t k = 0; k < nrd; k++)
          store (tkmap, rhc[k], wr[which], ddsi_serdata_ref (s), print && k == 0, true);
        ddsi_serdata_unref (s);
        break;
      }
      case 6:
        for (size_t k = 0; k < nrd; k++)
          rdall (rhc[k], NULL, print && k == 0, states_seen);
        break;
      case 7:
        for (size_t k = 0; k < nrd; k++)
          tkall (rhc[k], NULL, print && k == 0, states_seen);
        break;
      case 8: {
        uint32_t cond = ddsrt_prng_random (&prng) % (uint32_t) nconds;
        for (size_t k = 0; k < nrd; k++)
          rdcond (rhc[k], rhcconds[cond], NULL, 0, print && k == 0, states_seen);
        break;
      }
      case 9: {
        uint32_t cond = ddsrt_prng_random (&prng) % (uint32_t) nconds;
        for (size_t k = 0; k < nrd; k++)
          tkcond (rhc[k], rhcconds[cond], NULL, 0, print && k == 0, states_seen);
        break;
      }
      case 10: {
        uint32_t cond = ddsrt_prng_random (&prng) % (uint32_t) nconds;
        for (size_t k = 0; k < nrd; k++)
          tkcond (rhc[k], rhcconds[cond], NULL, 1, print && k == 0, states_seen);
        break;
      }
      case 11: {
        ddsi_thread_state_awake_domain_ok (ddsi_lookup_thread_state ());
        struct ddsi_writer_info wr_info;
        wr_info.auto_dispose = wr[which]->c.xqos->writer_data_lifecycle.autodispose_unregistered_instances;
        wr_info.guid = wr[which]->e.guid;
        wr_info.iid = wr[which]->e.iid;
        wr_info.ownership_strength = wr[which]->c.xqos->ownership_strength.value;
#ifdef DDS_HAS_LIFESPAN
        wr_info.lifespan_exp = DDSRT_MTIME_NEVER;
#endif
        for (size_t k = 0; k < nrd; k++)
          dds_rhc_unregister_wr (rhc[k], &wr_info);
        ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
        break;
      }
      case 12: {
#ifdef DDS_HAS_LIFESPAN
        ddsi_thread_state_awake_domain_ok (ddsi_lookup_thread_state ());
        /* We can assume that rhc[k] is a dds_rhc_default at this point */
        for (size_t k = 0; k < nrd; k++)
          (void) dds_rhc_default_sample_expired_cb (rhc[k], rand_texp());
        ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
#endif
        break;
      }
      case 13: {
#ifdef DDS_HAS_DEADLINE_MISSED
        ddsi_thread_state_awake_domain_ok (ddsi_lookup_thread_state ());
        /* We can assume that rhc[k] is a dds_rhc_default at this point */
        for (size_t k = 0; k < nrd; k++)
          (void) dds_rhc_default_deadline_missed_cb (rhc[k], rand_texp());
        ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
#endif
        break;
      }
    }

    if ((i % 200) == 0)
      wait_gc_cycle (gv->gcreq_queue);
  }

  for (size_t oper = 0; oper < sizeof (opcount) / sizeof (opcount[0]); oper++)
    printf ("%5s: %8"PRIu32"\n", operstr[oper], opcount[oper]);
  for (int i = 0; i < (int) (sizeof (states_seen) / sizeof (states_seen[0])); i++)
  {
    const char sst = (i & 1) ? 'N' : 'R';
    const char vst = (i & 2) ? 'N' : 'O';
    const char ist = (i >> 2) == 2 ? 'A' : (i >> 2) == 1 ? 'D' : 'U';
    printf ("%c%c%c: invalid %8"PRIu32" valid %8"PRIu32"\n", sst, vst, ist, states_seen[i][0], states_seen[i][1]);
  }

  dds_waitset_detach (waitset, gdcond);
  for (int ci = 0; ci < nconds; ci++)
    dds_waitset_detach (waitset, conds[ci]);
  dds_delete (waitset);
  dds_delete (gdcond);
  for (int ci = 0; ci < nconds; ci++)
    dds_delete (conds[ci]);
  for (size_t i = 0; i < nrd; i++)
    dds_delete (rd[i]);
  for (size_t i = 0, n = (sizeof (wr) / sizeof (wr[0])); i < n; i++)
    fwr (wr[i]);
}

struct stacktracethread_arg {
  dds_time_t when;
  dds_time_t period;
  bool stop;
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
};

static uint32_t stacktracethread (void *varg)
{
  struct stacktracethread_arg * const arg = varg;
  ddsrt_log_cfg_t logcfg;
  dds_time_t when = arg->when;
  dds_log_cfg_init (&logcfg, 0, ~0u, stdout, stdout);
  ddsrt_mutex_lock (&arg->lock);
  while (!arg->stop)
  {
    if (ddsrt_cond_waituntil (&arg->cond, &arg->lock, when))
      continue;
    ddsrt_mutex_unlock (&arg->lock);
    ddsi_log_stack_traces (&logcfg, NULL);
    ddsrt_mutex_lock (&arg->lock);
    if (arg->period == 0)
      break;
    if (when < INT64_MAX)
      when += arg->period;
  }
  ddsrt_mutex_unlock (&arg->lock);
  return 0;
}

int main (int argc, char **argv)
{
  ddsrt_init ();
  dds_entity_t pp = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  dds_entity_t tp = dds_create_topic(pp, &RhcTypes_T_desc, "RhcTypes_T", NULL, NULL);
  uint32_t states_seen[2 * 2 * 3][2] = {{ 0 }};
  unsigned seed = 0;
  bool print = false;
  int xchecks = 1;
  int first = 0, count = 10000;
  struct stacktracethread_arg sttarg = { 0 };
  ddsrt_thread_t stttid;
  memset (&stttid, 0, sizeof (stttid));

  ddsrt_mutex_init (&wait_gc_cycle_lock);
  ddsrt_cond_init (&wait_gc_cycle_cond);

  if (argc > 1)
    seed = (unsigned) atoi (argv[1]);
  if (seed == 0)
    seed = (unsigned) ddsrt_getpid ();
  if (argc > 2)
    first = atoi (argv[2]);
  if (argc > 3)
    count = atoi (argv[3]);
  if (argc > 4)
    print = (atoi (argv[4]) != 0);
  if (argc > 5)
    xchecks = atoi (argv[5]);
  if (argc > 6)
  {
    sttarg.when = dds_time () + DDS_SECS (atoi (argv[6]));
    sttarg.period = DDS_SECS (1);
    sttarg.stop = 0;
    ddsrt_mutex_init (&sttarg.lock);
    ddsrt_cond_init (&sttarg.cond);
    ddsrt_threadattr_t tattr;
    ddsrt_threadattr_init (&tattr);
    if (ddsrt_thread_create (&stttid, "stacktracethread", &tattr, stacktracethread, &sttarg) != 0)
      abort ();
  }

  printf ("%"PRId64" prng seed %u first %d count %d print %d xchecks %d\n", dds_time (), seed, first, count, print, xchecks);
  ddsrt_prng_init_simple (&prng, seed);

  if (xchecks != 0)
  {
    struct ddsi_domaingv *gv = get_gv (pp);
    if (xchecks > 0)
      gv->config.enabled_xchecks = ~0u;
    else
      gv->config.enabled_xchecks = 0u;
  }

  memset (rres_mseq, 0, sizeof (rres_mseq));
  for (size_t i = 0; i < sizeof (rres_iseq) / sizeof(rres_iseq[0]); i++)
    rres_ptrs[i] = &rres_mseq[i];

  tref_dds = dds_time();
  mainthread = ddsi_lookup_thread_state ();
  assert (ddsrt_atomic_ldvoidp (&mainthread->gv) != NULL);
  {
    struct dds_topic *x;
    if (dds_topic_pin (tp, &x) < 0) abort();
    mdtype = ddsi_sertype_ref (x->m_stype);
    dds_topic_unpin (x);
  }

  if (0 >= first)
  {
    struct ddsi_domaingv *gv = get_gv (pp);
    struct ddsi_tkmap *tkmap = gv->m_tkmap;
    printf ("%"PRId64" ************* 0 *************\n", dds_time ());
    struct dds_rhc *rhc = mkrhc (gv, NULL, DDS_HISTORY_KEEP_LAST, 1, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);
    struct ddsi_proxy_writer *wr0 = mkwr (1);
    struct ddsi_proxy_writer *wr1 = mkwr (1);
    uint64_t iid0, iid1, iid_t;
    iid0 = store (tkmap, rhc, wr0, mksample (0, 0), print, false);
    iid1 = store (tkmap, rhc, wr0, mksample (1, DDSI_STATUSINFO_DISPOSE), print, false);
    const struct check c0[] = {
      { "NNA", iid0, wr0->e.iid, 0,0, 1, 0,1 },
      { "NND", iid1, wr0->e.iid, 0,0, 1, 1,2 },
      { 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    rdall (rhc, c0, print, states_seen);
    /* write instance 0 with 2nd writer to have 2 live writers */
    iid_t = store (tkmap, rhc, wr1, mksample (0, 0), print, false);
    assert (iid_t == iid0);
    const struct check c1[] = {
      { "NOA", iid0, wr1->e.iid, 0,0, 1, 0,3 },
      { "ROD", iid1, wr0->e.iid, 0,0, 1, 1,2 },
      { 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    rdall (rhc, c1, print, states_seen);
    /* unregister instance 0 with wr0 - autodispose, but 2nd writer keeps it alive, no visible change */
    iid_t = store (tkmap, rhc, wr0, mkkeysample (0, DDSI_STATUSINFO_UNREGISTER), print, false);
    assert (iid_t == iid0);
    const struct check c2[] = {
      { "ROA", iid0, wr1->e.iid, 0,0, 1, 0,3 },
      { "ROD", iid1, wr0->e.iid, 0,0, 1, 1,2 },
      { 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    rdall (rhc, c2, print, states_seen);
    /* unregistering instance 0 again should be a no-op because wr0 no longer has it registered */
    iid_t = store (tkmap, rhc, wr0, mkkeysample (0, DDSI_STATUSINFO_UNREGISTER), print, false);
    assert (iid_t == iid0);
    rdall (rhc, c2, print, states_seen);
    /* unregistering instance 0 with wr1 - autodispose, no live writers -> dispose */
    iid_t = store (tkmap, rhc, wr1, mkkeysample (0, DDSI_STATUSINFO_UNREGISTER), print, false);
    assert (iid_t == iid0);
    const struct check c3[] = {
      { "ROD", iid0, wr1->e.iid, 0,0, 1, 0,3 },
      { "NOD", iid0, 0, 0,0, 0, 0,0 },
      { "ROD", iid1, wr0->e.iid, 0,0, 1, 1,2 },
      { 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    rdall (rhc, c3, print, states_seen);
    ddsi_thread_state_awake_domain_ok (ddsi_lookup_thread_state ());
    struct ddsi_writer_info wr0_info;
    wr0_info.auto_dispose = wr0->c.xqos->writer_data_lifecycle.autodispose_unregistered_instances;
    wr0_info.guid = wr0->e.guid;
    wr0_info.iid = wr0->e.iid;
    wr0_info.ownership_strength = wr0->c.xqos->ownership_strength.value;
#ifdef DDS_HAS_LIFESPAN
    wr0_info.lifespan_exp = DDSRT_MTIME_NEVER;
#endif
    dds_rhc_unregister_wr (rhc, &wr0_info);
    ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
    const struct check c4[] = {
      { "ROD", iid0, wr1->e.iid, 0,0, 1, 0,3 },
      { "ROD", iid0, 0, 0,0, 0, 0,0 },
      { "ROD", iid1, wr0->e.iid, 0,0, 1, 1,2 },
      // { "NOD", iid1, 0, 0,0, 0, 1,0 }, doesn't exist because it is already disposed
      { 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    tkall (rhc, c4, print, states_seen);
    frhc (rhc);
    fwr (wr0);
    fwr (wr1);
    (void)iid_t;
  }

  if (1 >= first)
  {
    struct ddsi_domaingv *gv = get_gv (pp);
    struct ddsi_tkmap *tkmap = gv->m_tkmap;
    printf ("%"PRId64" ************* 1 *************\n", dds_time ());
    struct dds_rhc *rhc = mkrhc (gv, NULL, DDS_HISTORY_KEEP_LAST, 4, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);
    struct ddsi_proxy_writer *wr[] = { mkwr (0), mkwr (0), mkwr (0) };
    uint64_t iid0, iid_t;
    int nregs = 3, isreg[] = { 1, 1, 1 };
    iid0 = store (tkmap, rhc, wr[0], mksample (0, 0), print, false);
    iid_t = store (tkmap, rhc, wr[1], mksample (0, 0), print, false); assert (iid0 == iid_t);
    iid_t = store (tkmap, rhc, wr[2], mksample (0, 0), print, false); assert (iid0 == iid_t);
    (void)iid0;
    tkall (rhc, NULL, print, states_seen);
    for (int i = 0; i < 3*3 * 3*3 * 3*3 * 3*3; i++)
    {
      for (int pos = 0, base = 1; pos < 3; pos++, base *= 3*3)
      {
        int which = (((i / base) / 3) + pos) % 3;
        int oper = (i / base) % 3;
        switch (oper)
        {
          case 0:
            iid_t = store (tkmap, rhc, wr[which], mksample (0, 0), print, false);
            if (!isreg[which]) { nregs++; isreg[which] = 1; }
            break;
          case 1:
            iid_t = store (tkmap, rhc, wr[which], mkkeysample (0, DDSI_STATUSINFO_DISPOSE), print, false);
            if (!isreg[which]) { nregs++; isreg[which] = 1; }
            break;
          case 2:
            if (nregs > 1 || !isreg[which])
            {
              iid_t = store (tkmap, rhc, wr[which], mkkeysample (0, DDSI_STATUSINFO_UNREGISTER), print, false);
              if (isreg[which]) { isreg[which] = 0; nregs--; }
            }
            break;
        }
        assert (iid_t == iid0);
      }
    }
    tkall (rhc, 0, print, states_seen);
    wait_gc_cycle (gv->gcreq_queue);
    assert (nregs > 0);
    for (int i = 0; i < 3; i++)
    {
      if (isreg[i])
      {
        iid_t = store (tkmap, rhc, wr[i], mkkeysample (0, DDSI_STATUSINFO_UNREGISTER), print, false);
        assert (iid_t == iid0);
        isreg[i] = 0;
        nregs--;
      }
    }
    assert (nregs == 0);
    tkall (rhc, 0, print, states_seen);
    wait_gc_cycle (gv->gcreq_queue);
    iid_t = store (tkmap, rhc, wr[0], mksample (0, 0), print, false);
    assert (iid_t != iid0);
    iid0 = iid_t;
    iid_t = store (tkmap, rhc, wr[0], mkkeysample (0, DDSI_STATUSINFO_UNREGISTER), print, false);
    assert (iid_t == iid0);
    frhc (rhc);

    for (size_t i = 0; i < sizeof (wr) / sizeof (wr[0]); i++)
      fwr (wr[i]);
  }

  {
    static const struct {
      dds_entity_t (*create) (dds_entity_t, uint32_t, dds_querycondition_filter_fn);
      dds_querycondition_filter_fn filter0;
      dds_querycondition_filter_fn filter1;
    } zztab[] = {
      { readcond_wrapper, 0, 0 },
      { dds_create_querycondition, qcpred_key, qcpred_attr2 },
      { dds_create_querycondition, qcpred_attr2, qcpred_attr3 }
    };
    for (int zz = 0; zz < (int) (sizeof (zztab) / sizeof (zztab[0])); zz++)
      if (zz + 2 >= first)
      {
        printf ("%"PRId64" ************* %d *************\n", dds_time (), zz + 2);
        test_conditions (pp, tp, count, zztab[zz].create, zztab[zz].filter0, zztab[zz].filter1, print);
      }
  }
  printf ("%"PRId64" cleaning up\n", dds_time ());

  ddsrt_cond_destroy (&wait_gc_cycle_cond);
  ddsrt_mutex_destroy (&wait_gc_cycle_lock);

  for (size_t i = 0; i < sizeof (rres_iseq) / sizeof (rres_iseq[0]); i++)
    RhcTypes_T_free (&rres_mseq[i], DDS_FREE_CONTENTS);

  ddsi_sertype_unref (mdtype);
  dds_delete (pp);

  if (sttarg.when)
  {
    ddsrt_mutex_lock (&sttarg.lock);
    sttarg.stop = 1;
    ddsrt_cond_signal (&sttarg.cond);
    ddsrt_mutex_unlock (&sttarg.lock);
    (void) ddsrt_thread_join (stttid, NULL);
    ddsrt_cond_destroy (&sttarg.cond);
    ddsrt_mutex_destroy (&sttarg.lock);
  }
  ddsrt_fini ();
  return 0;
}
