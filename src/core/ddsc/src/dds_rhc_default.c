// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>
#include <limits.h>

#if HAVE_VALGRIND && ! defined (NDEBUG)
#include <memcheck.h>
#define USE_VALGRIND 1
#else
#define USE_VALGRIND 0
#endif

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/time.h"

#include "dds__entity.h"
#include "dds__reader.h"
#include "dds/ddsc/dds_rhc.h"
#include "dds__rhc_default.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/circlist.h"
#include "dds/ddsi/ddsi_rhc.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_radmin.h" /* sampleinfo */
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_serdata.h"
#ifdef DDS_HAS_LIFESPAN
#include "dds/ddsi/ddsi_lifespan.h"
#endif
#ifdef DDS_HAS_DEADLINE_MISSED
#include "dds/ddsi/ddsi_deadline.h"
#endif

/* INSTANCE MANAGEMENT
   ===================

   Instances are created implicitly by "write" and "dispose", unregistered by
   "unregister".  Valid samples are added only by write operations (possibly
   a combined with dispose and/or unregister), invalid samples only by dispose
   and unregister operations, and only when there is no sample or the latest
   available sample is read.  (This might be a bit funny in the oddish case
   where someone would take only the latest of multiple valid samples.)

   There is at most one invalid sample per instance, its sample info is taken
   straight from the instance when it is returned to the reader and its
   presence and sample_state are represented by two bits.  Any incoming sample
   (or "incoming invalid sample") will cause an existing invalid sample to be
   dropped.  Thus, invalid samples are used solely to signal an instance state
   change when there are no samples.

   (Note: this can fairly easily be changed to let an invalid sample always
   be generated on dispose/unregister.)

   The instances and the RHC as a whole keep track of the number of valid
   samples and the number of read valid samples, as well as the same for the
   invalid ones, with the twist that the "inv_exists" and "inv_isread" booleans
   in the RHC serve as flags and as counters at the same time.

   Instances are dropped when the number of samples (valid & invalid combined)
   and the number of registrations both go to 0.  The number of registrations
   is kept track of in "wrcount", and a unique identifier for the most recent
   writer is typically in "wr_iid".  Typically, because an unregister by
   "wr_iid" clears it.  The actual set of registrations is in principle a set
   of <instance,writer> tuples stored in "registrations", but excluded from it
   are those instances that have "wrcount" = 1 and "wr_iid" != 0.  The typical
   case is a single active writer for an instance, and this means the typical
   case has no <instance,writer> tuples in "registrations".

   It is unfortunate that this model complicates the transitions from 1 writer
   to 2 writers, and from 2 writers back to 1 writer.  This seems a reasonable
   price to pay for the significant performance gain from not having to do
   anything in the case of a single (or single dominant) writer.

   (Note: "registrations" may perhaps be moved to a global registration table
   of <reader,instance,writer> tuples, using a lock-free hash table, but that
   doesn't affect the model.)

   The unique identifiers for instances and writers are approximately uniformly
   drawn from the set of positive unsigned 64-bit integers.  This means they
   are excellent hash keys, and both the instance hash table and the writer
   registrations hash table use these directly.

   QOS SUPPORT
   ===========

   History is implemented as a (circular) linked list, but the invalid samples
   model implemented here allows this to trivially be changed to an array of
   samples, and this is probably profitable for shallow histories.  Currently
   the instance has a single sample embedded in particular to optimise the
   KEEP_LAST with depth=1 case.

   BY_SOURCE ordering is implemented differently from OpenSplice and does not
   perform back-filling of the history.  The arguments against that can be
   found in JIRA, but in short: (1) not backfilling is significantly simpler
   (and thus faster), (2) backfilling potentially requires rewriting the
   states of samples already read, (3) it is as much "eventually consistent",
   the only difference is that the model implemented here considers the
   dataspace to fundamentally be "keep last 1" and always move forward (source
   timestamp increases), and the per-reader history to be a record of sampling
   that dataspace by the reader, whereas with backfilling the model is that
   the eventual consistency applies to the full history.

   (As it happens, the model implemented here is that also used by RTI and
   probably other implementations -- OpenSplice is the odd one out in this
   regard.)

   Exclusive ownership is implemented by dropping all data from all writers
   other than "wr_iid", unless "wr_iid" is 0 or the strength of the arriving
   sample is higher than the current strength of the instance (in "strength").
   The writer id is only reset by unregistering, in which case it is natural
   that ownership is up for grabs again.  QoS changes (not supported in this
   DDSI implementation, but still) will be done by also reseting "wr_iid"
   when an exclusive ownership writer lowers its strength.

   Lifespan is based on the reception timestamp, and the monotonic time is
   used for sample expiry if this QoS is set to something else than infinite.

   READ CONDITIONS
   ===============

   Read conditions are currently *always* attached to the reader, creating a
   read condition and not attaching it to a waitset is a bit of a waste of
   resources.  This can be changed, of course, but it is doubtful many read
   conditions get created without actually being used.

   The "trigger" of a read condition counts the number of instances
   matching its condition and is synchronously updated whenever the state
   of instances and/or samples changes. The instance/sample states are
   reduced to a triplet of a bitmask representing the instance and view
   states, whether or not the instance has unread samples, and whether or not
   it has read ones. (Invalid samples included.) Two of these triplets,
   pre-change and post-change are passed to "update_conditions_locked",
   which then runs over the array of attached read conditions and updates
   the trigger. It returns whether or not a trigger changed
   from 0 to 1, as this indicates the attached waitsets must be signalled.
*/

/* FIXME: tkmap should perhaps retain data with timestamp set to invalid
   An invalid timestamp is (logically) unordered with respect to valid
   timestamps, and that would mean BY_SOURCE order could be respected
   even when generating an invalid sample for an unregister message using
   the tkmap data. */

#define MAX_ATTACHED_QUERYCONDS (CHAR_BIT * sizeof (dds_querycond_mask_t))

#define INCLUDE_TRACE 1
#if INCLUDE_TRACE
#define TRACE(...) DDS_CLOG (DDS_LC_RHC, &rhc->gv->logconfig, __VA_ARGS__)
#else
#define TRACE(...) ((void)0)
#endif

/******************************
 ******   LIVE WRITERS   ******
 ******************************/

struct lwreg
{
  uint64_t iid;
  uint64_t wr_iid;
};

struct lwregs
{
  struct ddsrt_ehh * regs;
};

static uint32_t lwreg_hash (const void *vl)
{
  const struct lwreg * l = vl;
  return (uint32_t) (l->iid ^ l->wr_iid);
}

static int lwreg_equals (const void *va, const void *vb)
{
  const struct lwreg * a = va;
  const struct lwreg * b = vb;
  return a->iid == b->iid && a->wr_iid == b->wr_iid;
}

static void lwregs_init (struct lwregs *rt)
{
  rt->regs = NULL;
}

static void lwregs_fini (struct lwregs *rt)
{
  if (rt->regs)
    ddsrt_ehh_free (rt->regs);
}

static int lwregs_contains (struct lwregs *rt, uint64_t iid, uint64_t wr_iid)
{
  struct lwreg dummy = { .iid = iid, .wr_iid = wr_iid };
  return rt->regs != NULL && ddsrt_ehh_lookup (rt->regs, &dummy) != NULL;
}

static int lwregs_add (struct lwregs *rt, uint64_t iid, uint64_t wr_iid)
{
  struct lwreg dummy = { .iid = iid, .wr_iid = wr_iid };
  if (rt->regs == NULL)
    rt->regs = ddsrt_ehh_new (sizeof (struct lwreg), 1, lwreg_hash, lwreg_equals);
  return ddsrt_ehh_add (rt->regs, &dummy);
}

static int lwregs_delete (struct lwregs *rt, uint64_t iid, uint64_t wr_iid)
{
  struct lwreg dummy = { .iid = iid, .wr_iid = wr_iid };
  return rt->regs != NULL && ddsrt_ehh_remove (rt->regs, &dummy);
}

#if 0
void lwregs_dump (struct lwregs *rt)
{
  struct ddsrt_ehh_iter it;
  for (struct lwreg *r = ddsrt_ehh_iter_first(rt->regs, &it); r; r = ddsrt_ehh_iter_next(&it))
    printf("iid=%"PRIu64" wr_iid=%"PRIu64"\n", r->iid, r->wr_iid);
}
#endif

/*************************
 ******     RHC     ******
 *************************/

struct rhc_sample {
  struct ddsi_serdata *sample; /* serialised data (either just_key or real data) */
  struct rhc_sample *next;     /* next sample in time ordering, or oldest sample if most recent */
  uint64_t wr_iid;             /* unique id for writer of this sample (perhaps better in serdata) */
  dds_querycond_mask_t conds;  /* matching query conditions */
  bool isread;                 /* READ or NOT_READ sample state */
  uint32_t disposed_gen;       /* snapshot of instance counter at time of insertion */
  uint32_t no_writers_gen;     /* __/ */
#ifdef DDS_HAS_LIFESPAN
  struct ddsi_lifespan_fhnode lifespan;  /* fibheap node for lifespan */
  struct rhc_instance *inst;   /* reference to rhc instance */
#endif
};

struct rhc_instance {
  uint64_t iid;                /* unique instance id, key of table, also serves as instance handle */
  uint64_t wr_iid;             /* unique of id of writer of latest sample or 0; if wrcount = 0 it is the wr_iid that caused  */
  struct rhc_sample *latest;   /* latest received sample; circular list old->new; null if no sample */
  uint32_t nvsamples;          /* number of "valid" samples in instance */
  uint32_t nvread;             /* number of READ "valid" samples in instance (0 <= nvread <= nvsamples) */
  dds_querycond_mask_t conds;  /* matching query conditions */
  uint32_t wrcount;            /* number of live writers */
  unsigned isnew : 1;          /* NEW or NOT_NEW view state */
  unsigned a_sample_free : 1;  /* whether or not a_sample is in use */
  unsigned isdisposed : 1;     /* DISPOSED or NOT_DISPOSED (if not disposed, wrcount determines ALIVE/NOT_ALIVE_NO_WRITERS) */
  unsigned autodispose : 1;    /* wrcount > 0 => at least one registered writer has had auto-dispose set on some update */
  unsigned wr_iid_islive : 1;  /* whether wr_iid is of a live writer */
  unsigned inv_exists : 1;     /* whether or not state change occurred since last sample (i.e., must return invalid sample) */
  unsigned inv_isread : 1;     /* whether or not that state change has been read before */
  unsigned deadline_reg : 1;   /* whether or not registered for a deadline (== isdisposed, except store() defers updates) */
  uint32_t disposed_gen;       /* bloody generation counters - worst invention of mankind */
  uint32_t no_writers_gen;     /* __/ */
  int32_t strength;            /* "current" ownership strength */
  ddsi_guid_t wr_guid;         /* guid of last writer (if wr_iid != 0 then wr_guid is the corresponding guid, else undef) */
  ddsrt_wctime_t tstamp;          /* source time stamp of last update */
  struct ddsrt_circlist_elem nonempty_list; /* links non-empty instances in arbitrary ordering */
#ifdef DDS_HAS_DEADLINE_MISSED
  struct deadline_elem deadline; /* element in deadline missed administration */
#endif
  struct ddsi_tkmap_instance *tk;/* backref into TK for unref'ing */
  struct rhc_sample a_sample;  /* pre-allocated storage for 1 sample */
};

typedef enum rhc_store_result {
  RHC_STORED,
  RHC_FILTERED,
  RHC_REJECTED
} rhc_store_result_t;

struct dds_rhc_default {
  struct dds_rhc common;
  struct ddsrt_hh *instances;
  struct ddsrt_circlist nonempty_instances; /* circular, points to most recently added one, NULL if none */
  struct lwregs registrations;       /* should be a global one (with lock-free lookups) */

  /* Instance/Sample maximums from resource limits QoS */

  int32_t max_instances; /* FIXME: probably better as uint32_t with MAX_UINT32 for unlimited */
  int32_t max_samples;   /* FIXME: probably better as uint32_t with MAX_UINT32 for unlimited */
  int32_t max_samples_per_instance; /* FIXME: probably better as uint32_t with MAX_UINT32 for unlimited */
  dds_duration_t minimum_separation; /* derived from the time_based_filter QoSPolicy */

  uint32_t n_instances;              /* # instances, including empty */
  uint32_t n_nonempty_instances;     /* # non-empty instances */
  uint32_t n_not_alive_disposed;     /* # disposed, non-empty instances */
  uint32_t n_not_alive_no_writers;   /* # not-alive-no-writers, non-empty instances */
  uint32_t n_new;                    /* # new, non-empty instances */
  uint32_t n_vsamples;               /* # "valid" samples over all instances */
  uint32_t n_vread;                  /* # read "valid" samples over all instances */
  uint32_t n_invsamples;             /* # invalid samples over all instances */
  uint32_t n_invread;                /* # read invalid samples over all instances */

  bool by_source_ordering;           /* true if BY_SOURCE, false if BY_RECEPTION */
  bool exclusive_ownership;          /* true if EXCLUSIVE, false if SHARED */
  bool reliable;                     /* true if reliability RELIABLE */
  bool xchecks;                      /* whether to do expensive checking if checking at all */

  dds_reader *reader;                /* reader -- may be NULL (used by rhc_torture) */
  struct ddsi_tkmap *tkmap;          /* back pointer to tkmap */
  struct ddsi_domaingv *gv;          /* globals -- so far only for log config */
  const struct ddsi_sertype *type;   /* type description */
  uint32_t history_depth;            /* depth, 1 for KEEP_LAST_1, 2**32-1 for KEEP_ALL */

  ddsrt_mutex_t lock;
  dds_readcond * conds;              /* List of associated read conditions */
  uint32_t nconds;                   /* Number of associated read conditions */
  uint32_t nqconds;                  /* Number of associated query conditions */
  dds_querycond_mask_t qconds_samplest;  /* Mask of associated query conditions that check the sample state */
  void *qcond_eval_samplebuf;        /* Temporary storage for evaluating query conditions, NULL if no qconds */
#ifdef DDS_HAS_LIFESPAN
  struct ddsi_lifespan_adm lifespan;      /* Lifespan administration */
#endif
#ifdef DDS_HAS_DEADLINE_MISSED
  struct ddsi_deadline_adm deadline; /* Deadline missed administration */
#endif
};

struct trigger_info_cmn {
  uint32_t qminst;
  bool has_read;
  bool has_not_read;
};

struct trigger_info_pre {
  struct trigger_info_cmn c;
};

struct trigger_info_qcond {
  /* 0 or inst->conds depending on whether an invalid/valid sample was pushed out/added;
     inc_xxx_read is there so read can indicate a sample changed from unread to read */
  bool dec_invsample_read;
  bool dec_sample_read;
  bool inc_invsample_read;
  bool inc_sample_read;
  dds_querycond_mask_t dec_conds_invsample;
  dds_querycond_mask_t dec_conds_sample;
  dds_querycond_mask_t inc_conds_invsample;
  dds_querycond_mask_t inc_conds_sample;
};

struct trigger_info_post {
  struct trigger_info_cmn c;
};

static const struct dds_rhc_ops dds_rhc_default_ops;

static uint32_t qmask_of_sample (const struct rhc_sample *s)
{
  return s->isread ? DDS_READ_SAMPLE_STATE : DDS_NOT_READ_SAMPLE_STATE;
}

static uint32_t qmask_of_invsample (const struct rhc_instance *i)
{
  return i->inv_isread ? DDS_READ_SAMPLE_STATE : DDS_NOT_READ_SAMPLE_STATE;
}

static uint32_t inst_nsamples (const struct rhc_instance *i)
{
  return i->nvsamples + i->inv_exists;
}

static uint32_t inst_nread (const struct rhc_instance *i)
{
  return i->nvread + (uint32_t) (i->inv_exists & i->inv_isread);
}

static bool inst_is_empty (const struct rhc_instance *i)
{
  return inst_nsamples (i) == 0;
}

static bool inst_has_read (const struct rhc_instance *i)
{
  return inst_nread (i) > 0;
}

static bool inst_has_unread (const struct rhc_instance *i)
{
  return inst_nread (i) < inst_nsamples (i);
}

static bool untyped_to_clean_invsample (const struct ddsi_sertype *type, const struct ddsi_serdata *d, void *sample, void **bufptr, void *buflim)
{
  /* ddsi_serdata_untyped_to_sample just deals with the key value, without paying any attention to attributes;
     but that makes life harder for the user: the attributes of an invalid sample would be garbage, but would
     nonetheless have to be freed in the end.  Zero'ing it explicitly solves that problem. */
  ddsi_sertype_free_sample (type, sample, DDS_FREE_CONTENTS);
  ddsi_sertype_zero_sample (type, sample);
  return ddsi_serdata_untyped_to_sample (type, d, sample, bufptr, buflim);
}

static uint32_t qmask_of_inst (const struct rhc_instance *inst);
static void free_sample (struct dds_rhc_default *rhc, struct rhc_instance *inst, struct rhc_sample *s);
static void get_trigger_info_cmn (struct trigger_info_cmn *info, struct rhc_instance *inst);
static void get_trigger_info_pre (struct trigger_info_pre *info, struct rhc_instance *inst);
static void init_trigger_info_qcond (struct trigger_info_qcond *qc);
static void drop_instance_noupdate_no_writers (struct dds_rhc_default * __restrict rhc, struct rhc_instance * __restrict * __restrict instptr);
static bool update_conditions_locked (struct dds_rhc_default *rhc, bool called_from_insert, const struct trigger_info_pre *pre, const struct trigger_info_post *post, const struct trigger_info_qcond *trig_qc, const struct rhc_instance *inst);
static void account_for_nonempty_to_empty_transition (struct dds_rhc_default * __restrict rhc, struct rhc_instance * __restrict * __restrict instptr, const char *__restrict traceprefix);
#ifndef NDEBUG
static int rhc_check_counts_locked (struct dds_rhc_default *rhc, bool check_conds, bool check_qcmask);
#endif

static uint32_t instance_iid_hash (const void *va)
{
  const struct rhc_instance *a = va;
  return (uint32_t) a->iid;
}

static int instance_iid_eq (const void *va, const void *vb)
{
  const struct rhc_instance *a = va;
  const struct rhc_instance *b = vb;
  return (a->iid == b->iid);
}

static void add_inst_to_nonempty_list (struct dds_rhc_default *rhc, struct rhc_instance *inst)
{
  ddsrt_circlist_append (&rhc->nonempty_instances, &inst->nonempty_list);
  rhc->n_nonempty_instances++;
}

static void remove_inst_from_nonempty_list (struct dds_rhc_default *rhc, struct rhc_instance *inst)
{
  assert (inst_is_empty (inst));
  ddsrt_circlist_remove (&rhc->nonempty_instances, &inst->nonempty_list);
  assert (rhc->n_nonempty_instances > 0);
  rhc->n_nonempty_instances--;
}

static struct rhc_instance *oldest_nonempty_instance (const struct dds_rhc_default *rhc)
{
  return DDSRT_FROM_CIRCLIST (struct rhc_instance, nonempty_list, ddsrt_circlist_oldest (&rhc->nonempty_instances));
}

static struct rhc_instance *latest_nonempty_instance (const struct dds_rhc_default *rhc)
{
  return DDSRT_FROM_CIRCLIST (struct rhc_instance, nonempty_list, ddsrt_circlist_latest (&rhc->nonempty_instances));
}

static struct rhc_instance *next_nonempty_instance (const struct rhc_instance *inst)
{
  return DDSRT_FROM_CIRCLIST (struct rhc_instance, nonempty_list, inst->nonempty_list.next);
}

#ifdef DDS_HAS_LIFESPAN
static void drop_expired_samples (struct dds_rhc_default *rhc, struct rhc_sample *sample)
{
  struct rhc_instance *inst = sample->inst;
  struct trigger_info_pre pre;
  struct trigger_info_post post;
  struct trigger_info_qcond trig_qc;

  assert (!inst_is_empty (inst));

  TRACE ("rhc_default %p drop_exp(iid %"PRIx64" wriid %"PRIx64" exp %"PRId64" %s",
    rhc, inst->iid, sample->wr_iid, sample->lifespan.t_expire.v, sample->isread ? "read" : "notread");

  get_trigger_info_pre (&pre, inst);
  init_trigger_info_qcond (&trig_qc);

  /* Find prev sample: in case of history depth of 1 this is the sample itself,
    * (which is inst->latest). In case of larger history depth the most likely sample
    * to be expired is the oldest, in which case inst->latest is the previous
    * sample and inst->latest->next points to sample (circular list). We can
    * assume that 'sample' is in the list, so a check to avoid infinite loop is not
    * required here. */
  struct rhc_sample *psample = inst->latest;
  while (psample->next != sample)
    psample = psample->next;

  rhc->n_vsamples--;
  if (sample->isread)
  {
    inst->nvread--;
    rhc->n_vread--;
    trig_qc.dec_sample_read = true;
  }
  if (--inst->nvsamples > 0)
  {
    if (inst->latest == sample)
      inst->latest = psample;
    psample->next = sample->next;
  }
  else
  {
    inst->latest = NULL;
  }
  trig_qc.dec_conds_sample = sample->conds;
  free_sample (rhc, inst, sample);
  get_trigger_info_cmn (&post.c, inst);
  update_conditions_locked (rhc, false, &pre, &post, &trig_qc, inst);
  if (inst_is_empty (inst))
    account_for_nonempty_to_empty_transition(rhc, &inst, "; ");
  TRACE (")\n");
}

ddsrt_mtime_t dds_rhc_default_sample_expired_cb(void *hc, ddsrt_mtime_t tnow)
{
  struct dds_rhc_default *rhc = hc;
  struct rhc_sample *sample;
  ddsrt_mtime_t tnext;
  ddsrt_mutex_lock (&rhc->lock);
  while ((tnext = ddsi_lifespan_next_expired_locked (&rhc->lifespan, tnow, (void **)&sample)).v == 0)
    drop_expired_samples (rhc, sample);
  ddsrt_mutex_unlock (&rhc->lock);
  return tnext;
}
#endif /* DDS_HAS_LIFESPAN */

#ifdef DDS_HAS_DEADLINE_MISSED
ddsrt_mtime_t dds_rhc_default_deadline_missed_cb(void *hc, ddsrt_mtime_t tnow)
{
  struct dds_rhc_default *rhc = hc;
  ddsrt_mtime_t tnext = {0};
  uint32_t ninst = 0;
  void *vinst;
  ddsrt_mutex_lock (&rhc->lock);
  // stop after touching all instances to somewhat gracefully handle cases where we can't keep up
  // alternatively one could do at most a fixed number at the time
  while (ninst++ < rhc->n_instances && (tnext = ddsi_deadline_next_missed_locked (&rhc->deadline, tnow, &vinst)).v == 0)
  {
    struct rhc_instance *inst = vinst;
    const uint32_t deadlines_expired = ddsi_deadline_compute_deadlines_missed (tnow, &inst->deadline, rhc->deadline.dur);
    ddsi_deadline_reregister_instance_locked (&rhc->deadline, &inst->deadline, tnow);

    inst->wr_iid_islive = 0;

    ddsi_status_cb_data_t cb_data;
    cb_data.raw_status_id = (int) DDS_REQUESTED_DEADLINE_MISSED_STATUS_ID;
    cb_data.extra = deadlines_expired;
    cb_data.handle = inst->iid;
    cb_data.add = true;
    ddsrt_mutex_unlock (&rhc->lock);
    dds_reader_status_cb (&rhc->reader->m_entity, &cb_data);
    ddsrt_mutex_lock (&rhc->lock);

    tnow = ddsrt_time_monotonic ();
  }
  ddsrt_mutex_unlock (&rhc->lock);
  return tnext;
}
#endif /* DDS_HAS_DEADLINE_MISSED */

struct dds_rhc *dds_rhc_default_new_xchecks (dds_reader *reader, struct ddsi_domaingv *gv, const struct ddsi_sertype *type, bool xchecks)
{
  struct dds_rhc_default *rhc = ddsrt_malloc (sizeof (*rhc));
  memset (rhc, 0, sizeof (*rhc));
  rhc->common.common.ops = &dds_rhc_default_ops;

  lwregs_init (&rhc->registrations);
  ddsrt_mutex_init (&rhc->lock);
  rhc->instances = ddsrt_hh_new (1, instance_iid_hash, instance_iid_eq);
  ddsrt_circlist_init (&rhc->nonempty_instances);
  rhc->type = type;
  rhc->reader = reader;
  rhc->tkmap = gv->m_tkmap;
  rhc->gv = gv;
  rhc->xchecks = xchecks;

#ifdef DDS_HAS_LIFESPAN
  ddsi_lifespan_init (gv, &rhc->lifespan, offsetof(struct dds_rhc_default, lifespan), offsetof(struct rhc_sample, lifespan), dds_rhc_default_sample_expired_cb);
#endif

#ifdef DDS_HAS_DEADLINE_MISSED
  rhc->deadline.dur = (reader != NULL) ? reader->m_entity.m_qos->deadline.deadline : DDS_INFINITY;
  ddsi_deadline_init (gv, &rhc->deadline, offsetof(struct dds_rhc_default, deadline), offsetof(struct rhc_instance, deadline), dds_rhc_default_deadline_missed_cb);
#endif

  return &rhc->common;
}

struct dds_rhc *dds_rhc_default_new (dds_reader *reader, const struct ddsi_sertype *type)
{
  return dds_rhc_default_new_xchecks (reader, &reader->m_entity.m_domain->gv, type, (reader->m_entity.m_domain->gv.config.enabled_xchecks & DDSI_XCHECK_RHC) != 0);
}

static dds_return_t dds_rhc_default_associate (struct dds_rhc *rhc, dds_reader *reader, const struct ddsi_sertype *type, struct ddsi_tkmap *tkmap)
{
  /* ignored out of laziness */
  (void) rhc; (void) reader; (void) type; (void) tkmap;
  return DDS_RETCODE_OK;
}

static void dds_rhc_default_set_qos (struct ddsi_rhc *rhc_common, const dds_qos_t * qos)
{
  struct dds_rhc_default * const rhc = (struct dds_rhc_default *) rhc_common;
  /* Set read related QoS */

  rhc->max_samples = qos->resource_limits.max_samples;
  rhc->max_instances = qos->resource_limits.max_instances;
  rhc->max_samples_per_instance = qos->resource_limits.max_samples_per_instance;
  rhc->minimum_separation = qos->time_based_filter.minimum_separation;
  rhc->by_source_ordering = (qos->destination_order.kind == DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);
  rhc->exclusive_ownership = (qos->ownership.kind == DDS_OWNERSHIP_EXCLUSIVE);
  rhc->reliable = (qos->reliability.kind == DDS_RELIABILITY_RELIABLE);
  assert(qos->history.kind != DDS_HISTORY_KEEP_LAST || qos->history.depth > 0);
  rhc->history_depth = (qos->history.kind == DDS_HISTORY_KEEP_LAST) ? (uint32_t)qos->history.depth : ~0u;
  /* FIXME: updating deadline duration not yet supported
  rhc->deadline.dur = qos->deadline.deadline; */
}

static bool eval_predicate_sample (const struct dds_rhc_default *rhc, const struct ddsi_serdata *sample, bool (*pred) (const void *sample))
{
  ddsi_serdata_to_sample (sample, rhc->qcond_eval_samplebuf, NULL, NULL);
  bool ret = pred (rhc->qcond_eval_samplebuf);
  return ret;
}

static bool eval_predicate_invsample (const struct dds_rhc_default *rhc, const struct rhc_instance *inst, bool (*pred) (const void *sample))
{
  untyped_to_clean_invsample (rhc->type, inst->tk->m_sample, rhc->qcond_eval_samplebuf, NULL, NULL);
  bool ret = pred (rhc->qcond_eval_samplebuf);
  return ret;
}

static struct rhc_sample *alloc_sample (struct rhc_instance *inst)
{
  if (inst->a_sample_free)
  {
    inst->a_sample_free = 0;
#if USE_VALGRIND
    VALGRIND_MAKE_MEM_UNDEFINED (&inst->a_sample, sizeof (inst->a_sample));
#endif
    return &inst->a_sample;
  }
  else
  {
    /* This instead of sizeof(rhc_sample) gets us type checking */
    struct rhc_sample *s;
    s = ddsrt_malloc (sizeof (*s));
    return s;
  }
}

static void free_sample (struct dds_rhc_default *rhc, struct rhc_instance *inst, struct rhc_sample *s)
{
#ifndef DDS_HAS_LIFESPAN
  DDSRT_UNUSED_ARG (rhc);
#endif
  ddsi_serdata_unref (s->sample);
#ifdef DDS_HAS_LIFESPAN
  ddsi_lifespan_unregister_sample_locked (&rhc->lifespan, &s->lifespan);
#endif
  if (s == &inst->a_sample)
  {
    assert (!inst->a_sample_free);
#if USE_VALGRIND
    VALGRIND_MAKE_MEM_NOACCESS (&inst->a_sample, sizeof (inst->a_sample));
#endif
    inst->a_sample_free = 1;
  }
  else
  {
    ddsrt_free (s);
  }
}

static void inst_clear_invsample (struct dds_rhc_default *rhc, struct rhc_instance *inst, struct trigger_info_qcond *trig_qc)
{
  assert (inst->inv_exists);
  assert (trig_qc->dec_conds_invsample == 0);
  inst->inv_exists = 0;
  trig_qc->dec_conds_invsample = inst->conds;
  if (inst->inv_isread)
  {
    trig_qc->dec_invsample_read = true;
    rhc->n_invread--;
  }
  rhc->n_invsamples--;
}

static void inst_clear_invsample_if_exists (struct dds_rhc_default *rhc, struct rhc_instance *inst, struct trigger_info_qcond *trig_qc)
{
  if (inst->inv_exists)
    inst_clear_invsample (rhc, inst, trig_qc);
}

static void inst_set_invsample (struct dds_rhc_default *rhc, struct rhc_instance *inst, struct trigger_info_qcond *trig_qc, bool * __restrict nda)
{
  if (inst->inv_exists && !inst->inv_isread)
  {
    /* FIXME: should this indeed trigger a "notify data available" event?*/
    *nda = true;
  }
  else
  {
    /* Obviously optimisable, but that is perhaps not worth the bother */
    inst_clear_invsample_if_exists (rhc, inst, trig_qc);
    assert (trig_qc->inc_conds_invsample == 0);
    trig_qc->inc_conds_invsample = inst->conds;
    inst->inv_exists = 1;
    inst->inv_isread = 0;
    rhc->n_invsamples++;
    *nda = true;
  }
}

static void free_empty_instance (struct rhc_instance *inst, struct dds_rhc_default *rhc)
{
  assert (inst_is_empty (inst));
  ddsi_tkmap_instance_unref (rhc->tkmap, inst->tk);
#ifdef DDS_HAS_DEADLINE_MISSED
  if (inst->deadline_reg)
    ddsi_deadline_unregister_instance_locked (&rhc->deadline, &inst->deadline);
#endif
  ddsrt_free (inst);
}

static void free_instance_rhc_free (struct rhc_instance *inst, struct dds_rhc_default *rhc)
{
  struct rhc_sample *s = inst->latest;
  const bool was_empty = inst_is_empty (inst);
  struct trigger_info_qcond dummy_trig_qc;

  if (s)
  {
    do {
      struct rhc_sample * const s1 = s->next;
      free_sample (rhc, inst, s);
      s = s1;
    } while (s != inst->latest);
    rhc->n_vsamples -= inst->nvsamples;
    rhc->n_vread -= inst->nvread;
    inst->nvsamples = 0;
    inst->nvread = 0;
  }
#ifndef NDEBUG
  memset (&dummy_trig_qc, 0, sizeof (dummy_trig_qc));
#endif
  inst_clear_invsample_if_exists (rhc, inst, &dummy_trig_qc);
  if (!was_empty)
    remove_inst_from_nonempty_list (rhc, inst);
  if (inst->isnew)
    rhc->n_new--;
  free_empty_instance(inst, rhc);
}

static uint32_t dds_rhc_default_lock_samples (struct dds_rhc *rhc_common)
{
  struct dds_rhc_default * const rhc = (struct dds_rhc_default *) rhc_common;
  uint32_t no;
  ddsrt_mutex_lock (&rhc->lock);
  no = rhc->n_vsamples + rhc->n_invsamples;
  if (no == 0)
  {
    ddsrt_mutex_unlock (&rhc->lock);
  }
  return no;
}

static void free_instance_rhc_free_wrap (void *vnode, void *varg)
{
  free_instance_rhc_free (vnode, varg);
}

static void dds_rhc_default_free (struct ddsi_rhc *rhc_common)
{
  struct dds_rhc_default *rhc = (struct dds_rhc_default *) rhc_common;
#ifdef DDS_HAS_LIFESPAN
  dds_rhc_default_sample_expired_cb (rhc, DDSRT_MTIME_NEVER);
  ddsi_lifespan_fini (&rhc->lifespan);
#endif
#ifdef DDS_HAS_DEADLINE_MISSED
  ddsi_deadline_stop (&rhc->deadline);
#endif
  ddsrt_hh_enum (rhc->instances, free_instance_rhc_free_wrap, rhc);
  assert (ddsrt_circlist_isempty (&rhc->nonempty_instances));
#ifdef DDS_HAS_DEADLINE_MISSED
  ddsi_deadline_fini (&rhc->deadline);
#endif
  ddsrt_hh_free (rhc->instances);
  lwregs_fini (&rhc->registrations);
  if (rhc->qcond_eval_samplebuf != NULL)
    ddsi_sertype_free_sample (rhc->type, rhc->qcond_eval_samplebuf, DDS_FREE_ALL);
  ddsrt_mutex_destroy (&rhc->lock);
  ddsrt_free (rhc);
}

static void init_trigger_info_cmn_nonmatch (struct trigger_info_cmn *info)
{
  info->qminst = ~0u;
  info->has_read = false;
  info->has_not_read = false;
}

static void get_trigger_info_cmn (struct trigger_info_cmn *info, struct rhc_instance *inst)
{
  info->qminst = qmask_of_inst (inst);
  info->has_read = inst_has_read (inst);
  info->has_not_read = inst_has_unread (inst);
}

static void get_trigger_info_pre (struct trigger_info_pre *info, struct rhc_instance *inst)
{
  get_trigger_info_cmn (&info->c, inst);
}

static void init_trigger_info_qcond (struct trigger_info_qcond *qc)
{
  qc->dec_invsample_read = false;
  qc->dec_sample_read = false;
  qc->inc_invsample_read = false;
  qc->inc_sample_read = false;
  qc->dec_conds_invsample = 0;
  qc->dec_conds_sample = 0;
  qc->inc_conds_invsample = 0;
  qc->inc_conds_sample = 0;
}

static bool trigger_info_differs (const struct dds_rhc_default *rhc, const struct trigger_info_pre *pre, const struct trigger_info_post *post, const struct trigger_info_qcond *trig_qc)
{
  if (pre->c.qminst != post->c.qminst ||
      pre->c.has_read != post->c.has_read ||
      pre->c.has_not_read != post->c.has_not_read)
    return true;
  else if (rhc->nqconds == 0)
    return false;
  else
    return (trig_qc->dec_conds_invsample != trig_qc->inc_conds_invsample ||
            trig_qc->dec_conds_sample != trig_qc->inc_conds_sample ||
            trig_qc->dec_invsample_read != trig_qc->inc_invsample_read ||
            trig_qc->dec_sample_read != trig_qc->inc_sample_read);
}

static bool add_sample (struct dds_rhc_default *rhc, struct rhc_instance *inst, const struct ddsi_writer_info *wrinfo, const struct ddsi_serdata *sample, ddsi_status_cb_data_t *cb_data, struct trigger_info_qcond *trig_qc, bool * __restrict nda)
{
  struct rhc_sample *s;

  /* Adding a sample always clears an invalid sample (because the information
     contained in the invalid sample - the instance state and the generation
     counts - are included in the sample).  While this would be place to do it,
     we do it later to avoid having to roll back on allocation failure */

  /* We don't do backfilling in BY_SOURCE mode -- we could, but
     choose not to -- and having already filtered out samples
     preceding inst->latest, we can simply insert it without any
     searching */
  if (inst->nvsamples == rhc->history_depth)
  {
    /* replace oldest sample; latest points to the latest one, the
       list is circular from old -> new, so latest->next is the oldest */
    inst_clear_invsample_if_exists (rhc, inst, trig_qc);
    assert (inst->latest != NULL);
    s = inst->latest->next;
    assert (trig_qc->dec_conds_sample == 0);
    ddsi_serdata_unref (s->sample);

#ifdef DDS_HAS_LIFESPAN
    ddsi_lifespan_unregister_sample_locked (&rhc->lifespan, &s->lifespan);
#endif

    trig_qc->dec_sample_read = s->isread;
    trig_qc->dec_conds_sample = s->conds;
    if (s->isread)
    {
      inst->nvread--;
      rhc->n_vread--;
    }
  }
  else
  {
    /* Check if resource max_samples QoS exceeded */
    if (rhc->reader && rhc->max_samples != DDS_LENGTH_UNLIMITED && rhc->n_vsamples >= (uint32_t) rhc->max_samples)
    {
      cb_data->raw_status_id = (int) DDS_SAMPLE_REJECTED_STATUS_ID;
      cb_data->extra = DDS_REJECTED_BY_SAMPLES_LIMIT;
      cb_data->handle = inst->iid;
      cb_data->add = true;
      return false;
    }

    /* Check if resource max_samples_per_instance QoS exceeded */
    if (rhc->reader && rhc->max_samples_per_instance != DDS_LENGTH_UNLIMITED && inst->nvsamples >= (uint32_t) rhc->max_samples_per_instance)
    {
      cb_data->raw_status_id = (int) DDS_SAMPLE_REJECTED_STATUS_ID;
      cb_data->extra = DDS_REJECTED_BY_SAMPLES_PER_INSTANCE_LIMIT;
      cb_data->handle = inst->iid;
      cb_data->add = true;
      return false;
    }

    /* add new latest sample */
    s = alloc_sample (inst);
    inst_clear_invsample_if_exists (rhc, inst, trig_qc);
    if (inst->latest == NULL)
    {
      s->next = s;
    }
    else
    {
      s->next = inst->latest->next;
      inst->latest->next = s;
    }
    inst->nvsamples++;
    rhc->n_vsamples++;
  }

  s->sample = ddsi_serdata_ref (sample); /* drops const (tho refcount does change) */
  s->wr_iid = wrinfo->iid;
  s->isread = false;
  s->disposed_gen = inst->disposed_gen;
  s->no_writers_gen = inst->no_writers_gen;
#ifdef DDS_HAS_LIFESPAN
  s->inst = inst;
  s->lifespan.t_expire = wrinfo->lifespan_exp;
  ddsi_lifespan_register_sample_locked (&rhc->lifespan, &s->lifespan);
#endif

  s->conds = 0;
  if (rhc->nqconds != 0)
  {
    for (dds_readcond *rc = rhc->conds; rc != NULL; rc = rc->m_next)
      if (rc->m_query.m_filter != 0 && eval_predicate_sample (rhc, s->sample, rc->m_query.m_filter))
        s->conds |= rc->m_query.m_qcmask;
  }

  trig_qc->inc_conds_sample = s->conds;
  inst->latest = s;
  *nda = true;
  return true;
}

static void content_filter_make_sampleinfo (struct dds_sample_info *si, const struct ddsi_serdata *sample, const struct rhc_instance *inst, uint64_t wr_iid, uint64_t iid)
{
  si->sample_state = DDS_SST_NOT_READ;
  si->publication_handle = wr_iid;
  si->source_timestamp = sample->timestamp.v;
  si->sample_rank = 0;
  si->generation_rank = 0;
  si->absolute_generation_rank = 0;
  si->valid_data = true;
  if (inst)
  {
    si->view_state = inst->isnew ? DDS_VST_NEW : DDS_VST_OLD;
    si->instance_state = inst->isdisposed ? DDS_IST_NOT_ALIVE_DISPOSED : (inst->wrcount == 0) ? DDS_IST_NOT_ALIVE_NO_WRITERS : DDS_IST_ALIVE;
    si->instance_handle = inst->iid;
    si->disposed_generation_count = inst->disposed_gen;
    si->no_writers_generation_count = inst->no_writers_gen;
  }
  else
  {
    si->view_state = DDS_VST_NEW;
    si->instance_state = DDS_IST_ALIVE;
    si->instance_handle = iid;
    si->disposed_generation_count = 0;
    si->no_writers_generation_count = 0;
  }
}

static bool content_filter_accepts (const dds_reader *reader, const struct ddsi_serdata *sample, const struct rhc_instance *inst, uint64_t wr_iid, uint64_t iid)
{
  bool ret = true;
  if (reader)
  {
    const struct dds_topic *tp = reader->m_topic;
    switch (tp->m_filter.mode)
    {
      case DDS_TOPIC_FILTER_NONE:
        ret = true;
        break;
      case DDS_TOPIC_FILTER_SAMPLEINFO_ARG: {
        struct dds_sample_info si;
        content_filter_make_sampleinfo (&si, sample, inst, wr_iid, iid);
        ret = tp->m_filter.f.sampleinfo_arg (&si, tp->m_filter.arg);
        break;
      }
      case DDS_TOPIC_FILTER_SAMPLE:
      case DDS_TOPIC_FILTER_SAMPLE_ARG:
      case DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG: {
        char *tmp;
        tmp = ddsi_sertype_alloc_sample (tp->m_stype);
        ddsi_serdata_to_sample (sample, tmp, NULL, NULL);
        switch (tp->m_filter.mode)
        {
          case DDS_TOPIC_FILTER_NONE:
          case DDS_TOPIC_FILTER_SAMPLEINFO_ARG:
            assert (0);
          case DDS_TOPIC_FILTER_SAMPLE:
            ret = (tp->m_filter.f.sample) (tmp);
            break;
          case DDS_TOPIC_FILTER_SAMPLE_ARG:
            ret = (tp->m_filter.f.sample_arg) (tmp, tp->m_filter.arg);
            break;
          case DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG: {
            struct dds_sample_info si;
            content_filter_make_sampleinfo (&si, sample, inst, wr_iid, iid);
            ret = tp->m_filter.f.sample_sampleinfo_arg (tmp, &si, tp->m_filter.arg);
            break;
          }
        }
        ddsi_sertype_free_sample (tp->m_stype, tmp, DDS_FREE_ALL);
        break;
      }
    }
  }
  return ret;
}

static int inst_accepts_sample_by_writer_guid (const struct rhc_instance *inst, const struct ddsi_writer_info *wrinfo)
{
  return (inst->wr_iid_islive && inst->wr_iid == wrinfo->iid) || memcmp (&wrinfo->guid, &inst->wr_guid, sizeof (inst->wr_guid)) < 0;
}

static int inst_accepts_sample (const struct dds_rhc_default *rhc, const struct rhc_instance *inst, const struct ddsi_writer_info *wrinfo, const struct ddsi_serdata *sample, const bool has_data)
{
  if (rhc->by_source_ordering) {
    /* source ordering, so compare timestamps*/
    if (sample->timestamp.v == DDS_TIME_INVALID ||
        inst->tstamp.v == DDS_TIME_INVALID ||
        inst->tstamp.v == sample->timestamp.v) {
      /* one or both of the samples has no valid timestamp,
         or both are at the same time, writer guid check */
      if (!inst_accepts_sample_by_writer_guid (inst, wrinfo))
        return 0;
    } else if (sample->timestamp.v < inst->tstamp.v) {
      /* sample is before inst, so definitely reject */
      return 0;
    }
    /* sample is later than inst, further checks may be needed */
  }

  if (rhc->minimum_separation > 0 &&
      sample->timestamp.v != DDS_TIME_INVALID &&
      inst->tstamp.v != DDS_TIME_INVALID) {
    if (sample->timestamp.v < INT64_MIN + rhc->minimum_separation ||
        sample->timestamp.v - rhc->minimum_separation < inst->tstamp.v) {
      return 0;//reject
    }
  }

  if (rhc->exclusive_ownership && inst->wr_iid_islive && inst->wr_iid != wrinfo->iid)
  {
    int32_t strength = wrinfo->ownership_strength;
    if (strength > inst->strength) {
      /* ok */
    } else if (strength < inst->strength) {
      return 0;
    } else if (inst_accepts_sample_by_writer_guid (inst, wrinfo)) {
      /* ok */
    } else {
      return 0;
    }
  }
  if (has_data && !content_filter_accepts (rhc->reader, sample, inst, wrinfo->iid, inst->iid))
  {
    return 0;
  }
  return 1;
}

static void update_inst_common (struct rhc_instance *inst, const struct ddsi_writer_info * __restrict wrinfo, ddsrt_wctime_t tstamp)
{
  inst->tstamp = tstamp;
  inst->strength = wrinfo->ownership_strength;
}

static void update_inst_have_wr_iid (struct rhc_instance *inst, const struct ddsi_writer_info * __restrict wrinfo, ddsrt_wctime_t tstamp)
{
  update_inst_common (inst, wrinfo, tstamp);
  inst->wr_iid = wrinfo->iid;
  inst->wr_guid = wrinfo->guid;
  inst->wr_iid_islive = true;
}

static void update_inst_no_wr_iid (struct rhc_instance *inst, const struct ddsi_writer_info * __restrict wrinfo, ddsrt_wctime_t tstamp)
{
  update_inst_common (inst, wrinfo, tstamp);
  inst->wr_iid_islive = false;
}

static void drop_instance_noupdate_no_writers (struct dds_rhc_default *__restrict rhc, struct rhc_instance * __restrict * __restrict instptr)
{
  struct rhc_instance *inst = *instptr;
  assert (inst_is_empty (inst));

  rhc->n_instances--;
  if (inst->isnew)
    rhc->n_new--;

  ddsrt_hh_remove_present (rhc->instances, inst);
  free_empty_instance (inst, rhc);
  *instptr = NULL;
}

static void dds_rhc_register (struct dds_rhc_default *rhc, struct rhc_instance *inst, uint64_t wr_iid, bool autodispose, bool sample_accepted, bool * __restrict nda)
{
  const uint64_t inst_wr_iid = inst->wr_iid_islive ? inst->wr_iid : 0;

  TRACE (" register:");

  /* Is an implicitly registering dispose semantically equivalent to
     register ; dispose?  If so, both no_writers_gen and disposed_gen
     need to be incremented if the old instance state was DISPOSED,
     else just disposed_gen.  (Shudder.)  Interpreting it as
     equivalent.

     Is a dispose a sample?  I don't think so (though a write dispose
     is).  Is a pure register a sample?  Don't think so either. */
  if (inst_wr_iid == wr_iid)
  {
    /* Same writer as last time => we know it is registered already.
       This is the fast path -- we don't have to check anything
       else. */
    TRACE ("cached");
    assert (inst->wrcount > 0);
  }
  else if (inst->wrcount == 0)
  {
    /* Currently no writers at all */
    assert (!inst->wr_iid_islive);

    /* When registering a writer based on a rejected sample and causing
       the instance to transition from not-alive to alive, we do want
       to generate an invalid sample with the id of the newly registered
       (or re-registered) writer, but we don't want inst_accepts_sample
       to be affected (it was "too old" in the ordering).  wr_iid_islive
       determines whether wr_iid is meaningful, so setting wr_iid while
       leaving wr_iid_islive false gets us the desired behaviour. */
    inst->wr_iid = wr_iid;
    if (sample_accepted)
      inst->wr_iid_islive = 1;
    inst->wrcount++;
    inst->no_writers_gen++;
    inst->autodispose = autodispose;
    TRACE ("new1");

    if (!inst_is_empty (inst) && !inst->isdisposed)
      rhc->n_not_alive_no_writers--;
    *nda = true;
  }
  else if (inst_wr_iid == 0 && inst->wrcount == 1)
  {
    /* Writers exist, but wr_iid is null => someone unregistered.

       With wrcount 1, if wr_iid happens to be the remaining writer,
       we remove the explicit registration and once again rely on
       inst->wr_iid, but if wr_iid happens to be a new writer, we
       increment the writer count & explicitly register the second
       one, too.

       If I decide on a global table of registrations implemented
       using concurrent hopscotch-hashing, then this should still
       scale well because lwregs_add first calls lwregs_contains,
       which is lock-free.  (Not that it probably can't be optimised
       by a combined add-if-unknown-delete-if-known operation -- but
       the value of that is likely negligible because the
       registrations should be fairly stable.) */
    if (lwregs_add (&rhc->registrations, inst->iid, wr_iid))
    {
      inst->wrcount++;
      if (autodispose)
        inst->autodispose = 1;
      TRACE ("new2iidnull");
    }
    else
    {
      int x = lwregs_delete (&rhc->registrations, inst->iid, wr_iid);
      assert (x);
      (void) x;
      TRACE ("restore");
    }
    /* to avoid wr_iid update when register is called for sample rejected */
    if (sample_accepted)
    {
      inst->wr_iid = wr_iid;
      inst->wr_iid_islive = 1;
    }
  }
  else
  {
    /* As above -- if using concurrent hopscotch hashing, if the
       writer is already known, lwregs_add is lock-free */
    if (inst->wrcount == 1)
    {
      /* 2nd writer => properly register the one we knew about */
      TRACE ("rescue1");
      int x;
      x = lwregs_add (&rhc->registrations, inst->iid, inst_wr_iid);
      assert (x);
      (void) x;
    }
    if (lwregs_add (&rhc->registrations, inst->iid, wr_iid))
    {
      /* as soon as we reach at least two writers, we have to check
         the result of lwregs_add to know whether this sample
         registers a previously unknown writer or not */
      TRACE ("new3");
      inst->wrcount++;
      if (autodispose)
        inst->autodispose = 1;
    }
    else
    {
      TRACE ("known");
    }
    assert (inst->wrcount >= 2);
    /* the most recent writer gets the fast path */
    /* to avoid wr_iid update when register is called for sample rejected */
    if (sample_accepted)
    {
      inst->wr_iid = wr_iid;
      inst->wr_iid_islive = 1;
    }
  }
}

static void account_for_empty_to_nonempty_transition (struct dds_rhc_default *rhc, struct rhc_instance *inst)
{
  assert (inst_nsamples (inst) == 1);
  add_inst_to_nonempty_list (rhc, inst);
  if (inst->isdisposed)
    rhc->n_not_alive_disposed++;
  else if (inst->wrcount == 0)
    rhc->n_not_alive_no_writers++;
}

static void account_for_nonempty_to_empty_transition (struct dds_rhc_default *__restrict rhc, struct rhc_instance * __restrict * __restrict instptr, const char * __restrict traceprefix)
{
  struct rhc_instance *inst = *instptr;
  assert (inst_is_empty (inst));
  remove_inst_from_nonempty_list (rhc, inst);
  if (inst->isdisposed)
    rhc->n_not_alive_disposed--;
  if (inst->wrcount == 0)
  {
    TRACE ("%siid %"PRIx64" #0,empty,drop\n", traceprefix, inst->iid);
    if (!inst->isdisposed)
    {
      /* disposed has priority over no writers (why not just 2 bits?) */
      rhc->n_not_alive_no_writers--;
    }
    drop_instance_noupdate_no_writers (rhc, instptr);
  }
}

static int rhc_unregister_delete_registration (struct dds_rhc_default *rhc, const struct rhc_instance *inst, uint64_t wr_iid)
{
  /* Returns 1 if last registration just disappeared */
  if (inst->wrcount == 0)
  {
    TRACE ("unknown(#0)");
    return 0;
  }
  else if (inst->wrcount == 1 && inst->wr_iid_islive)
  {
    assert(inst->wr_iid != 0);
    if (wr_iid != inst->wr_iid)
    {
      TRACE ("unknown(cache)");
      return 0;
    }
    else
    {
      TRACE ("last(cache)");
      return 1;
    }
  }
  else if (!lwregs_delete (&rhc->registrations, inst->iid, wr_iid))
  {
    TRACE ("unknown(regs)");
    return 0;
  }
  else
  {
    TRACE ("delreg");
    /* If we transition from 2 to 1 writer, and we are deleting a
       writer other than the one cached in the instance, that means
       afterward there will be 1 writer, it will be cached, and its
       registration record must go (invariant that with wrcount = 1
       and wr_iid != 0 the wr_iid is not in "registrations") */
    if (inst->wrcount == 2 && inst->wr_iid_islive && inst->wr_iid != wr_iid)
    {
      TRACE (",delreg(remain)");
      (void) lwregs_delete (&rhc->registrations, inst->iid, inst->wr_iid);
    }
    return 1;
  }
}

static int rhc_unregister_updateinst (struct dds_rhc_default *rhc, struct rhc_instance *inst, const struct ddsi_writer_info * __restrict wrinfo, ddsrt_wctime_t tstamp, struct trigger_info_qcond *trig_qc, bool * __restrict nda)
{
  assert (inst->wrcount > 0);
  if (wrinfo->auto_dispose)
    inst->autodispose = 1;

  if (--inst->wrcount > 0)
  {
    if (inst->wr_iid_islive && wrinfo->iid == inst->wr_iid)
    {
      /* Next register will have to do real work before we have a cached
       wr_iid again */
      inst->wr_iid_islive = 0;

      /* Reset the ownership strength to allow samples to be read from other
       writer(s) */
      inst->strength = 0;
      TRACE (",clearcache");
    }
    return 0;
  }
  else
  {
    if (!inst_is_empty (inst))
    {
      /* Instance still has content - do not drop until application
         takes the last sample.  Set the invalid sample if the latest
         sample has been read already, so that the application can
         read the change to not-alive.  (If the latest sample is still
         unread, we don't bother, even though it means the application
         won't see the timestamp for the unregister event. It shouldn't
         care.) */
      if (!inst->isdisposed)
      {
        if (inst->latest == NULL || inst->latest->isread)
        {
          inst_set_invsample (rhc, inst, trig_qc, nda);
          update_inst_no_wr_iid (inst, wrinfo, tstamp);
        }
        if (!inst->autodispose)
          rhc->n_not_alive_no_writers++;
        else
        {
          TRACE (",autodispose");
          inst->isdisposed = 1;
          rhc->n_not_alive_disposed++;
        }
        *nda = true;
      }
      inst->wr_iid_islive = 0;
      return 0;
    }
    else if (inst->isdisposed)
    {
      /* No content left, no registrations left, so drop */
      TRACE (",#0,empty,nowriters,disposed");
      return 1;
    }
    else
    {
      /* Add invalid samples for transition to no-writers */
      TRACE (",#0,empty,nowriters");
      assert (inst_is_empty (inst));
      inst_set_invsample (rhc, inst, trig_qc, nda);
      update_inst_no_wr_iid (inst, wrinfo, tstamp);
      if (inst->autodispose)
      {
        TRACE (",autodispose");
        inst->isdisposed = 1;
      }
      account_for_empty_to_nonempty_transition (rhc, inst);
      inst->wr_iid_islive = 0;
      *nda = true;
      return 0;
    }
  }
}

static void dds_rhc_unregister (struct dds_rhc_default *rhc, struct rhc_instance *inst, const struct ddsi_writer_info * __restrict wrinfo, ddsrt_wctime_t tstamp, struct trigger_info_post *post, struct trigger_info_qcond *trig_qc, bool * __restrict nda)
{
  /* 'post' always gets set */
  TRACE (" unregister:");
  if (!rhc_unregister_delete_registration (rhc, inst, wrinfo->iid)) {
    /* other registrations remain */
    get_trigger_info_cmn (&post->c, inst);
  } else if (rhc_unregister_updateinst (rhc, inst, wrinfo, tstamp, trig_qc, nda)) {
    /* instance dropped */
    init_trigger_info_cmn_nonmatch (&post->c);
  } else {
    /* no writers remain, but instance not empty */
    get_trigger_info_cmn (&post->c, inst);
  }
  TRACE (" nda=%d\n", *nda);
}

static struct rhc_instance *alloc_new_instance (struct dds_rhc_default *rhc, const struct ddsi_writer_info *wrinfo, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk)
{
  struct rhc_instance *inst;

  ddsi_tkmap_instance_ref (tk);
  inst = ddsrt_malloc (sizeof (*inst));
  memset (inst, 0, sizeof (*inst));
  inst->iid = tk->m_iid;
  inst->tk = tk;
  inst->wrcount = 1;
  inst->isdisposed = (serdata->statusinfo & DDSI_STATUSINFO_DISPOSE) != 0;
  inst->autodispose = wrinfo->auto_dispose;
  inst->deadline_reg = 0;
  inst->isnew = 1;
  inst->a_sample_free = 1;
  inst->conds = 0;
  inst->wr_iid = wrinfo->iid;
  inst->wr_iid_islive = (inst->wrcount != 0);
  inst->wr_guid = wrinfo->guid;
  inst->tstamp = serdata->timestamp;
  inst->strength = wrinfo->ownership_strength;

  if (rhc->nqconds != 0)
  {
    for (dds_readcond *c = rhc->conds; c != NULL; c = c->m_next)
    {
      assert ((dds_entity_kind (&c->m_entity) == DDS_KIND_COND_READ && c->m_query.m_filter == 0) ||
              (dds_entity_kind (&c->m_entity) == DDS_KIND_COND_QUERY && c->m_query.m_filter != 0));
      if (c->m_query.m_filter && eval_predicate_invsample (rhc, inst, c->m_query.m_filter))
        inst->conds |= c->m_query.m_qcmask;
    }
  }
  return inst;
}

static rhc_store_result_t rhc_store_new_instance (struct rhc_instance **out_inst, struct dds_rhc_default *rhc, const struct ddsi_writer_info *wrinfo, struct ddsi_serdata *sample, struct ddsi_tkmap_instance *tk, const bool has_data, ddsi_status_cb_data_t *cb_data, struct trigger_info_qcond *trig_qc, bool * __restrict nda)
{
  struct rhc_instance *inst;
  int ret;

  /* New instance for this reader.  May still filter out key value.

     Doing the filtering here means avoiding filter processing in
     the normal case of accepting data, accepting some extra
     overhead in the case where the data would be filtered out.
     Naturally using an avl tree is not so smart for these IIDs, and
     if the AVL tree is replaced by a hash table, the overhead
     trade-off should be quite nice with the filtering code right
     here.

     Note: never instantiating based on a sample that's filtered out,
     though one could argue that if it is rejected based on an
     attribute (rather than a key), an empty instance should be
     instantiated. */

  if (has_data && !content_filter_accepts (rhc->reader, sample, NULL, wrinfo->iid, tk->m_iid))
  {
    return RHC_FILTERED;
  }
  /* Check if resource max_instances QoS exceeded */

  if (rhc->reader && rhc->max_instances != DDS_LENGTH_UNLIMITED && rhc->n_instances >= (uint32_t) rhc->max_instances)
  {
    cb_data->raw_status_id = (int) DDS_SAMPLE_REJECTED_STATUS_ID;
    cb_data->extra = DDS_REJECTED_BY_INSTANCES_LIMIT;
    cb_data->handle = tk->m_iid;
    cb_data->add = true;
    return RHC_REJECTED;
  }

  inst = alloc_new_instance (rhc, wrinfo, sample, tk);
  if (has_data)
  {
    if (!add_sample (rhc, inst, wrinfo, sample, cb_data, trig_qc, nda))
    {
      free_empty_instance (inst, rhc);
      return RHC_REJECTED;
    }
  }
  else
  {
    if (inst->isdisposed)
      inst_set_invsample (rhc, inst, trig_qc, nda);
  }

  account_for_empty_to_nonempty_transition (rhc, inst);
  ret = ddsrt_hh_add (rhc->instances, inst);
  assert (ret);
  (void) ret;
  rhc->n_instances++;
  rhc->n_new++;

  *out_inst = inst;
  return RHC_STORED;
}

static void postprocess_instance_update (struct dds_rhc_default * __restrict rhc, struct rhc_instance * __restrict * __restrict instptr, const struct trigger_info_pre *pre, const struct trigger_info_post *post, struct trigger_info_qcond *trig_qc)
{
  {
    struct rhc_instance *inst = *instptr;

#ifdef DDS_HAS_DEADLINE_MISSED
    if (inst->isdisposed)
    {
      if (inst->deadline_reg)
      {
        inst->deadline_reg = 0;
        ddsi_deadline_unregister_instance_locked (&rhc->deadline, &inst->deadline);
      }
    }
    else
    {
      if (inst->deadline_reg)
        ddsi_deadline_renew_instance_locked (&rhc->deadline, &inst->deadline);
      else
      {
        ddsi_deadline_register_instance_locked (&rhc->deadline, &inst->deadline, ddsrt_time_monotonic ());
        inst->deadline_reg = 1;
      }
    }
#endif

    if (inst_is_empty (inst) && inst->wrcount == 0)
    {
      drop_instance_noupdate_no_writers (rhc, instptr);
    }
  }

  if (trigger_info_differs (rhc, pre, post, trig_qc))
    update_conditions_locked (rhc, true, pre, post, trig_qc, *instptr);

  assert (rhc_check_counts_locked (rhc, true, true));
}

static void update_viewstate_and_disposedness (struct dds_rhc_default * __restrict rhc, struct rhc_instance * __restrict inst, bool has_data, bool not_alive, bool is_dispose, bool * __restrict nda)
{
  /* Sample arriving for a NOT_ALIVE instance => view state NEW */
  if (has_data && not_alive)
  {
    TRACE (" notalive->alive");
    inst->isnew = 1;
    *nda = true;
  }

  /* Desired effect on instance state and disposed_gen:
       op     DISPOSED    NOT_DISPOSED
       W      ND;gen++    ND
       D      D           D
       WD     D;gen++     D
     Simplest way is to toggle istate when it is currently DISPOSED
     and the operation is WD. */
  if (has_data && inst->isdisposed)
  {
    TRACE (" disposed->notdisposed");
    inst->disposed_gen++;
    if (!is_dispose)
      inst->isdisposed = 0;
    *nda = true;
  }
  if (is_dispose)
  {
    bool wasdisposed = inst->isdisposed;
    if (!inst->isdisposed)
    {
      inst->isdisposed = 1;
      *nda = true;
    }
    TRACE (" dispose(%d)", !wasdisposed);
  }
}

/*
  dds_rhc_store: DDSI up call into read cache to store new sample. Returns whether sample
  delivered (true unless a reliable sample rejected).
*/

static bool dds_rhc_default_store (struct ddsi_rhc * __restrict rhc_common, const struct ddsi_writer_info * __restrict wrinfo, struct ddsi_serdata * __restrict sample, struct ddsi_tkmap_instance * __restrict tk)
{
  struct dds_rhc_default * const __restrict rhc = (struct dds_rhc_default * __restrict) rhc_common;
  const uint64_t wr_iid = wrinfo->iid;
  const uint32_t statusinfo = sample->statusinfo;
  const bool has_data = (sample->kind == SDK_DATA);
  const int is_dispose = (statusinfo & DDSI_STATUSINFO_DISPOSE) != 0;
  struct rhc_instance dummy_instance;
  struct rhc_instance *inst;
  struct trigger_info_pre pre;
  struct trigger_info_post post;
  struct trigger_info_qcond trig_qc;
  rhc_store_result_t stored;
  ddsi_status_cb_data_t cb_data;   /* Callback data for reader status callback */
  bool notify_data_available;

  TRACE ("rhc_store %"PRIx64",%"PRIx64" si %"PRIx32" has_data %d:", tk->m_iid, wr_iid, statusinfo, has_data);
  if (!has_data && statusinfo == 0)
  {
    /* Write with nothing but a key -- I guess that would be a
       register, which we do implicitly. (Currently DDSI2 won't allow
       it through anyway.) */
    TRACE (" ignore explicit register\n");
    return true;
  }

  notify_data_available = false;
  dummy_instance.iid = tk->m_iid;
  stored = RHC_FILTERED;
  cb_data.raw_status_id = -1;

  init_trigger_info_qcond (&trig_qc);

  ddsrt_mutex_lock (&rhc->lock);

  inst = ddsrt_hh_lookup (rhc->instances, &dummy_instance);
  if (inst == NULL)
  {
    /* New instance for this reader.  If no data content -- not (also)
       a write -- ignore it, I think we can get away with ignoring dispose or unregisters
       on unknown instances.
     */
    if (!has_data && !is_dispose)
    {
      TRACE (" unreg on unknown instance\n");
      goto error_or_nochange;
    }
    else
    {
      TRACE (" new instance\n");
      stored = rhc_store_new_instance (&inst, rhc, wrinfo, sample, tk, has_data, &cb_data, &trig_qc, &notify_data_available);
      if (stored != RHC_STORED)
        goto error_or_nochange;

      init_trigger_info_cmn_nonmatch (&pre.c);
    }
  }
  else if (!inst_accepts_sample (rhc, inst, wrinfo, sample, has_data))
  {
    /* Rejected samples (and disposes) should still register the writer;
       unregister *must* be processed, or we have a memory leak. (We
       will raise a SAMPLE_REJECTED, and indicate that the system should
       kill itself.)  Not letting instances go to ALIVE or NEW based on
       a rejected sample - (no one knows, it seemed) */
    TRACE (" instance rejects sample\n");

    get_trigger_info_pre (&pre, inst);
    if (has_data || is_dispose)
    {
      dds_rhc_register (rhc, inst, wr_iid, wrinfo->auto_dispose, false, &notify_data_available);
      if (notify_data_available)
      {
        if (inst->latest == NULL || inst->latest->isread)
        {
          const bool was_empty = inst_is_empty (inst);
          inst_set_invsample (rhc, inst, &trig_qc, &notify_data_available);
          if (was_empty)
            account_for_empty_to_nonempty_transition (rhc, inst);
        }
      }
    }

    /* notify sample lost */
    cb_data.raw_status_id = (int) DDS_SAMPLE_LOST_STATUS_ID;
    cb_data.extra = 0;
    cb_data.handle = 0;
    cb_data.add = true;
  }
  else
  {
    get_trigger_info_pre (&pre, inst);

    TRACE (" wc %"PRIu32, inst->wrcount);

    if (has_data || is_dispose)
    {
      /* View state must be NEW following receipt of a sample when
         instance was NOT_ALIVE (whether DISPOSED or NO_WRITERS).
         Once we start fiddling with the state, we can no longer
         figure out whether it is alive or not, so determine whether
         it is currently NOT_ALIVE. */
      const int not_alive = inst->wrcount == 0 || inst->isdisposed;
      const bool old_isdisposed = inst->isdisposed;
      const bool old_isnew = inst->isnew;
      const bool was_empty = inst_is_empty (inst);

      /* Not just an unregister, so a write and/or a dispose (possibly
         combined with an unregister).  Write & dispose create a
         registration and we always do that, even if we have to delete
         it immediately afterward.  It seems unlikely to be worth the
         effort of optimising this, but it can be done.  On failure
         (i.e., out-of-memory), abort the operation and hope that the
         caller can still notify the application.  */

      dds_rhc_register (rhc, inst, wr_iid, wrinfo->auto_dispose, true, &notify_data_available);
      update_viewstate_and_disposedness (rhc, inst, has_data, not_alive, is_dispose, &notify_data_available);

      /* Only need to add a sample to the history if the input actually is a sample. */
      if (has_data)
      {
        TRACE (" add_sample");
        if (!add_sample (rhc, inst, wrinfo, sample, &cb_data, &trig_qc, &notify_data_available))
        {
          TRACE ("(reject)\n");
          stored = RHC_REJECTED;

          /* FIXME: fix the bad rejection handling, probably put back in a proper rollback, until then a band-aid like this will have to do: */
          inst->isnew = old_isnew;
          if (old_isdisposed)
            inst->disposed_gen--;
          inst->isdisposed = old_isdisposed;
          goto error_or_nochange;
        }
      }

      /* If instance became disposed, add an invalid sample if there are no samples left */
      if ((bool) inst->isdisposed > old_isdisposed && (inst->latest == NULL || inst->latest->isread))
        inst_set_invsample (rhc, inst, &trig_qc, &notify_data_available);

      update_inst_have_wr_iid (inst, wrinfo, sample->timestamp);

      /* Can only add samples => only need to give special treatment
         to instances that were empty before.  It is, however, not
         guaranteed that we end up with a non-empty instance: for
         example, if the instance was disposed & empty, nothing
         changes. */
      if (inst->latest || (bool) inst->isdisposed > old_isdisposed)
      {
        if (was_empty)
          account_for_empty_to_nonempty_transition (rhc, inst);
        else
          rhc->n_not_alive_disposed += (uint32_t)(inst->isdisposed - old_isdisposed);
        rhc->n_new += (uint32_t)(inst->isnew - old_isnew);
      }
      else
      {
        assert (inst_is_empty (inst) == was_empty);
      }
    }

    TRACE(" nda=%d\n", notify_data_available);
    assert (rhc_check_counts_locked (rhc, false, false));
  }

  if (statusinfo & DDSI_STATUSINFO_UNREGISTER)
  {
    /* Either a pure unregister, or the instance rejected the sample
       because of time stamps, content filter, or something else.  If
       the writer unregisters the instance, I think we should ignore
       the acceptance filters and process it anyway.

       It is a bit unclear what

         write_w_timestamp(x,1) ; unregister_w_timestamp(x,0)

       actually means if BY_SOURCE ordering is selected: does that
       mean an application reading "x" after the write and reading it
       again after the unregister will see a change in the
       no_writers_generation field? */
    dds_rhc_unregister (rhc, inst, wrinfo, sample->timestamp, &post, &trig_qc, &notify_data_available);
  }
  else
  {
    get_trigger_info_cmn (&post.c, inst);
  }

  postprocess_instance_update (rhc, &inst, &pre, &post, &trig_qc);

error_or_nochange:
  ddsrt_mutex_unlock (&rhc->lock);

  if (rhc->reader)
  {
    if (notify_data_available)
      dds_reader_data_available_cb (rhc->reader);
    if (cb_data.raw_status_id >= 0)
      dds_reader_status_cb (&rhc->reader->m_entity, &cb_data);
  }
  return !(rhc->reliable && stored == RHC_REJECTED);
}

static void dds_rhc_default_unregister_wr (struct ddsi_rhc * __restrict rhc_common, const struct ddsi_writer_info * __restrict wrinfo)
{
  /* Only to be called when writer with ID WR_IID has died.

     If we require that it will NEVER be resurrected, i.e., that next
     time a new WR_IID will be used for the same writer, then we have
     all the time in the world to scan the cache & clean up and that
     we don't have to keep it locked all the time (even if we do it
     that way now).

     WR_IID was never reused while the built-in topics weren't getting
     generated, but those really require the same instance id for the
     same GUID if an instance still exists in some reader for that GUID.
     So, if unregistration without locking the RHC is desired, entities
     need to get two IIDs: the one visible to the application in the
     built-in topics and in get_instance_handle, and one used internally
     for tracking registrations and unregistrations. */
  struct dds_rhc_default * __restrict const rhc = (struct dds_rhc_default * __restrict) rhc_common;
  bool notify_data_available = false;
  struct rhc_instance *inst;
  struct ddsrt_hh_iter iter;
  const uint64_t wr_iid = wrinfo->iid;

  ddsrt_mutex_lock (&rhc->lock);
  TRACE ("rhc_unregister_wr_iid %"PRIx64",%d:\n", wr_iid, wrinfo->auto_dispose);
  for (inst = ddsrt_hh_iter_first (rhc->instances, &iter); inst; inst = ddsrt_hh_iter_next (&iter))
  {
    if ((inst->wr_iid_islive && inst->wr_iid == wr_iid) || lwregs_contains (&rhc->registrations, inst->iid, wr_iid))
    {
      assert (inst->wrcount > 0);
      struct trigger_info_pre pre;
      struct trigger_info_post post;
      struct trigger_info_qcond trig_qc;
      get_trigger_info_pre (&pre, inst);
      init_trigger_info_qcond (&trig_qc);
      TRACE ("  %"PRIx64":", inst->iid);
      dds_rhc_unregister (rhc, inst, wrinfo, inst->tstamp, &post, &trig_qc, &notify_data_available);
      postprocess_instance_update (rhc, &inst, &pre, &post, &trig_qc);
      TRACE ("\n");
    }
  }
  ddsrt_mutex_unlock (&rhc->lock);

  if (rhc->reader && notify_data_available)
    dds_reader_data_available_cb (rhc->reader);
}

static void dds_rhc_default_relinquish_ownership (struct ddsi_rhc * __restrict rhc_common, const uint64_t wr_iid)
{
  struct dds_rhc_default * __restrict const rhc = (struct dds_rhc_default * __restrict) rhc_common;
  struct rhc_instance *inst;
  struct ddsrt_hh_iter iter;
  ddsrt_mutex_lock (&rhc->lock);
  TRACE ("rhc_relinquish_ownership(%"PRIx64":\n", wr_iid);
  for (inst = ddsrt_hh_iter_first (rhc->instances, &iter); inst; inst = ddsrt_hh_iter_next (&iter))
  {
    if (inst->wr_iid_islive && inst->wr_iid == wr_iid)
    {
      inst->wr_iid_islive = 0;
    }
  }
  TRACE (")\n");
  assert (rhc_check_counts_locked (rhc, true, false));
  ddsrt_mutex_unlock (&rhc->lock);
}

/* STATUSES:

   sample:   ANY, READ, NOT_READ
   view:     ANY, NEW, NOT_NEW
   instance: ANY, ALIVE, NOT_ALIVE, NOT_ALIVE_NO_WRITERS, NOT_ALIVE_DISPOSED
*/

static uint32_t qmask_of_inst (const struct rhc_instance *inst)
{
  uint32_t qm = inst->isnew ? DDS_NEW_VIEW_STATE : DDS_NOT_NEW_VIEW_STATE;

  if (inst->isdisposed)
    qm |= DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE;
  else if (inst->wrcount > 0)
    qm |= DDS_ALIVE_INSTANCE_STATE;
  else
    qm |= DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE;

  return qm;
}

static uint32_t qmask_from_dcpsquery (uint32_t sample_states, uint32_t view_states, uint32_t instance_states)
{
  uint32_t qminv = 0;

  switch ((dds_sample_state_t) sample_states)
  {
    case DDS_SST_READ:
      qminv |= DDS_NOT_READ_SAMPLE_STATE;
      break;
    case DDS_SST_NOT_READ:
      qminv |= DDS_READ_SAMPLE_STATE;
      break;
  }
  switch ((dds_view_state_t) view_states)
  {
    case DDS_VST_NEW:
      qminv |= DDS_NOT_NEW_VIEW_STATE;
      break;
    case DDS_VST_OLD:
      qminv |= DDS_NEW_VIEW_STATE;
      break;
  }
  switch (instance_states)
  {
    case DDS_IST_ALIVE:
      qminv |= DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE;
      break;
    case DDS_IST_ALIVE | DDS_IST_NOT_ALIVE_DISPOSED:
      qminv |= DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE;
      break;
    case DDS_IST_ALIVE | DDS_IST_NOT_ALIVE_NO_WRITERS:
      qminv |= DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE;
      break;
    case DDS_IST_NOT_ALIVE_DISPOSED:
      qminv |= DDS_ALIVE_INSTANCE_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE;
      break;
    case DDS_IST_NOT_ALIVE_DISPOSED | DDS_IST_NOT_ALIVE_NO_WRITERS:
      qminv |= DDS_ALIVE_INSTANCE_STATE;
      break;
    case DDS_IST_NOT_ALIVE_NO_WRITERS:
      qminv |= DDS_ALIVE_INSTANCE_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE;
      break;
  }
  return qminv;
}

static uint32_t qmask_from_mask_n_cond (uint32_t mask, dds_readcond* cond)
{
    uint32_t qminv;
    if (mask == NO_STATE_MASK_SET) {
        if (cond) {
            /* No mask set, use the one from the condition. */
            qminv = cond->m_qminv;
        } else {
            /* No mask set and no condition: read all. */
            qminv = qmask_from_dcpsquery(DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
        }
    } else {
        /* Merge given mask with the condition mask when needed. */
        qminv = qmask_from_dcpsquery(mask & DDS_ANY_SAMPLE_STATE, mask & DDS_ANY_VIEW_STATE, mask & DDS_ANY_INSTANCE_STATE);
        if (cond != NULL) {
            qminv &= cond->m_qminv;
        }
    }
    return qminv;
}

static void set_sample_info (dds_sample_info_t *si, const struct rhc_instance *inst, const struct rhc_sample *sample)
{
  si->sample_state = sample->isread ? DDS_SST_READ : DDS_SST_NOT_READ;
  si->view_state = inst->isnew ? DDS_VST_NEW : DDS_VST_OLD;
  si->instance_state = inst->isdisposed ? DDS_IST_NOT_ALIVE_DISPOSED : (inst->wrcount == 0) ? DDS_IST_NOT_ALIVE_NO_WRITERS : DDS_IST_ALIVE;
  si->instance_handle = inst->iid;
  si->publication_handle = sample->wr_iid;
  si->disposed_generation_count = sample->disposed_gen;
  si->no_writers_generation_count = sample->no_writers_gen;
  si->sample_rank = 0;     /* patch afterward: don't know last sample in returned set yet */
  si->generation_rank = 0; /* __/ */
  si->absolute_generation_rank = (inst->disposed_gen + inst->no_writers_gen) - (sample->disposed_gen + sample->no_writers_gen);
  si->valid_data = true;
  si->source_timestamp = sample->sample->timestamp.v;
}

static void set_sample_info_invsample (dds_sample_info_t *si, const struct rhc_instance *inst)
{
  si->sample_state = inst->inv_isread ? DDS_SST_READ : DDS_SST_NOT_READ;
  si->view_state = inst->isnew ? DDS_VST_NEW : DDS_VST_OLD;
  si->instance_state = inst->isdisposed ? DDS_IST_NOT_ALIVE_DISPOSED : (inst->wrcount == 0) ? DDS_IST_NOT_ALIVE_NO_WRITERS : DDS_IST_ALIVE;
  si->instance_handle = inst->iid;
  si->publication_handle = inst->wr_iid;
  si->disposed_generation_count = inst->disposed_gen;
  si->no_writers_generation_count = inst->no_writers_gen;
  si->sample_rank = 0;     /* by construction always last in the set (but will get patched) */
  si->generation_rank = 0; /* __/ */
  si->absolute_generation_rank = 0;
  si->valid_data = false;
  si->source_timestamp = inst->tstamp.v;
}

static void patch_generations (dds_sample_info_t *si, uint32_t last_of_inst)
{
  if (last_of_inst > 0)
  {
    const uint32_t ref =
      si[last_of_inst].disposed_generation_count + si[last_of_inst].no_writers_generation_count;
    assert (si[last_of_inst].sample_rank == 0);
    assert (si[last_of_inst].generation_rank == 0);
    for (uint32_t i = 0; i < last_of_inst; i++)
    {
      si[i].sample_rank = last_of_inst - i;
      si[i].generation_rank = ref - (si[i].disposed_generation_count + si[i].no_writers_generation_count);
    }
  }
}

static bool read_sample_update_conditions (struct dds_rhc_default *rhc, struct trigger_info_pre *pre, struct trigger_info_post *post, struct trigger_info_qcond *trig_qc, struct rhc_instance *inst, dds_querycond_mask_t conds, bool sample_wasread)
{
  /* No query conditions that are dependent on sample states */
  if (rhc->qconds_samplest == 0)
    return false;

  /* Some, but perhaps none that matches this sample */
  if ((conds & rhc->qconds_samplest) == 0)
    return false;

  TRACE("read_sample_update_conditions\n");
  trig_qc->dec_conds_sample = trig_qc->inc_conds_sample = conds;
  trig_qc->dec_sample_read = sample_wasread;
  trig_qc->inc_sample_read = true;
  get_trigger_info_cmn (&post->c, inst);
  update_conditions_locked (rhc, false, pre, post, trig_qc, inst);
  trig_qc->dec_conds_sample = trig_qc->inc_conds_sample = 0;
  pre->c = post->c;
  return false;
}

static bool take_sample_update_conditions (struct dds_rhc_default *rhc, struct trigger_info_pre *pre, struct trigger_info_post *post, struct trigger_info_qcond *trig_qc, struct rhc_instance *inst, dds_querycond_mask_t conds, bool sample_wasread)
{
  /* Mostly the same as read_...: but we are deleting samples (so no "inc sample") and need to process all query conditions that match this sample. */
  if (rhc->nqconds == 0 || conds == 0)
    return false;

  TRACE("take_sample_update_conditions\n");
  trig_qc->dec_conds_sample = conds;
  trig_qc->dec_sample_read = sample_wasread;
  get_trigger_info_cmn (&post->c, inst);
  update_conditions_locked (rhc, false, pre, post, trig_qc, inst);
  trig_qc->dec_conds_sample = 0;
  pre->c = post->c;
  return false;
}

typedef bool (*read_take_to_sample_t) (const struct ddsi_serdata * __restrict d, void *__restrict  *__restrict  sample, void * __restrict * __restrict bufptr, void * __restrict buflim);
typedef bool (*read_take_to_invsample_t) (const struct ddsi_sertype * __restrict type, const struct ddsi_serdata * __restrict d, void *__restrict * __restrict sample, void * __restrict * __restrict bufptr, void * __restrict buflim);

static bool read_take_to_sample (const struct ddsi_serdata * __restrict d, void * __restrict * __restrict sample, void * __restrict * __restrict bufptr, void * __restrict buflim)
{
  return ddsi_serdata_to_sample (d, *sample, (void **) bufptr, buflim);
}

static bool read_take_to_invsample (const struct ddsi_sertype * __restrict type, const struct ddsi_serdata * __restrict d, void * __restrict * __restrict sample, void * __restrict * __restrict bufptr, void * __restrict buflim)
{
  return untyped_to_clean_invsample (type, d, *sample, (void **) bufptr, buflim);
}

static bool read_take_to_sample_ref (const struct ddsi_serdata * __restrict d, void * __restrict * __restrict sample, void * __restrict * __restrict bufptr, void * __restrict buflim)
{
  (void) bufptr; (void) buflim;
  *sample = ddsi_serdata_ref (d);
  return true;
}

static bool read_take_to_invsample_ref (const struct ddsi_sertype * __restrict type, const struct ddsi_serdata * __restrict d, void * __restrict * __restrict sample, void * __restrict * __restrict bufptr, void * __restrict buflim)
{
  (void) type; (void) bufptr; (void) buflim;
  *sample = ddsi_serdata_ref (d);
  return true;
}

static int32_t read_w_qminv_inst (struct dds_rhc_default * const __restrict rhc, struct rhc_instance * const __restrict inst, void * __restrict * __restrict values, dds_sample_info_t * __restrict info_seq, const int32_t max_samples, const uint32_t qminv, const dds_querycond_mask_t qcmask, read_take_to_sample_t to_sample, read_take_to_invsample_t to_invsample)
{
  assert (max_samples > 0);
  if (inst_is_empty (inst) || (qmask_of_inst (inst) & qminv) != 0)
  {
    /* no samples present, or the instance/view state doesn't match */
    return 0;
  }

  struct trigger_info_pre pre;
  struct trigger_info_post post;
  struct trigger_info_qcond trig_qc;
  const uint32_t nread = inst_nread (inst);
  int32_t n = 0;
  get_trigger_info_pre (&pre, inst);
  init_trigger_info_qcond (&trig_qc);

  /* any valid samples precede a possible invalid sample */
  if (inst->latest)
  {
    struct rhc_sample *sample = inst->latest->next, * const end1 = sample;
    do {
      if ((qmask_of_sample (sample) & qminv) == 0 && (qcmask == 0 || (sample->conds & qcmask)))
      {
        /* sample state matches too */
        set_sample_info (info_seq + n, inst, sample);
        to_sample (sample->sample, values + n, 0, 0);
        if (!sample->isread)
        {
          read_sample_update_conditions (rhc, &pre, &post, &trig_qc, inst, sample->conds, false);
          sample->isread = true;
          inst->nvread++;
          rhc->n_vread++;
        }
        ++n;
      }
      sample = sample->next;
    } while (n < max_samples && sample != end1);
  }

  /* add an invalid sample if it exists, matches and there is room in the result */
  if (inst->inv_exists && n < max_samples && (qmask_of_invsample (inst) & qminv) == 0 && (qcmask == 0 || (inst->conds & qcmask)))
  {
    set_sample_info_invsample (info_seq + n, inst);
    to_invsample (rhc->type, inst->tk->m_sample, values + n, 0, 0);
    if (!inst->inv_isread)
    {
      read_sample_update_conditions (rhc, &pre, &post, &trig_qc, inst, inst->conds, false);
      inst->inv_isread = 1;
      rhc->n_invread++;
    }
    ++n;
  }

  /* set generation counts in sample info now that we can compute them; update instance state */
  bool inst_became_old = false;
  if (n > 0)
  {
    patch_generations (info_seq, (uint32_t) n - 1);
    if (inst->isnew)
    {
      inst_became_old = true;
      inst->isnew = 0;
      rhc->n_new--;
    }
  }
  if (nread != inst_nread (inst) || inst_became_old)
  {
    get_trigger_info_cmn (&post.c, inst);
    assert (trig_qc.dec_conds_invsample == 0);
    assert (trig_qc.dec_conds_sample == 0);
    assert (trig_qc.inc_conds_invsample == 0);
    assert (trig_qc.inc_conds_sample == 0);
    update_conditions_locked (rhc, false, &pre, &post, &trig_qc, inst);
  }
  return n;
}

static int32_t take_w_qminv_inst (struct dds_rhc_default * const __restrict rhc, struct rhc_instance * __restrict * __restrict instptr, void * __restrict * __restrict values, dds_sample_info_t * __restrict info_seq, const int32_t max_samples, const uint32_t qminv, const dds_querycond_mask_t qcmask, read_take_to_sample_t to_sample, read_take_to_invsample_t to_invsample)
{
  struct rhc_instance *inst = *instptr;
  assert (max_samples > 0);
  if (inst_is_empty (inst) || (qmask_of_inst (inst) & qminv) != 0)
  {
    /* no samples present, or the instance/view state doesn't match */
    return 0;
  }

  struct trigger_info_pre pre;
  struct trigger_info_post post;
  struct trigger_info_qcond trig_qc;
  int32_t n = 0;
  get_trigger_info_pre (&pre, inst);
  init_trigger_info_qcond (&trig_qc);

  if (inst->latest)
  {
    struct rhc_sample *psample = inst->latest;
    struct rhc_sample *sample = psample->next;
    uint32_t nvsamples = inst->nvsamples;
    while (nvsamples--)
    {
      struct rhc_sample * const sample1 = sample->next;
      if ((qmask_of_sample (sample) & qminv) != 0 || (qcmask != 0 && !(sample->conds & qcmask)))
      {
        /* sample mask doesn't match, or content predicate doesn't match */
        psample = sample;
      }
      else
      {
        take_sample_update_conditions (rhc, &pre, &post, &trig_qc, inst, sample->conds, sample->isread);
        set_sample_info (info_seq + n, inst, sample);
        to_sample (sample->sample, values + n, 0, 0);
        rhc->n_vsamples--;
        if (sample->isread)
        {
          inst->nvread--;
          rhc->n_vread--;
        }
        if (--inst->nvsamples == 0)
          inst->latest = NULL;
        else
        {
          if (inst->latest == sample)
            inst->latest = psample;
          psample->next = sample1;
        }
        free_sample (rhc, inst, sample);
        if (++n == max_samples)
          break;
      }
      sample = sample1;
    }
  }

  if (inst->inv_exists && n < max_samples && (qmask_of_invsample (inst) & qminv) == 0 && (qcmask == 0 || (inst->conds & qcmask) != 0))
  {
    struct trigger_info_qcond dummy_trig_qc;
#ifndef NDEBUG
    init_trigger_info_qcond (&dummy_trig_qc);
#endif
    take_sample_update_conditions (rhc, &pre, &post, &trig_qc, inst, inst->conds, inst->inv_isread);
    set_sample_info_invsample (info_seq + n, inst);
    to_invsample (rhc->type, inst->tk->m_sample, values + n, 0, 0);
    inst_clear_invsample (rhc, inst, &dummy_trig_qc);
    ++n;
  }

  if (n > 0)
  {
    patch_generations (info_seq, (uint32_t) n - 1);
    if (inst->isnew)
    {
      inst->isnew = 0;
      rhc->n_new--;
    }
    /* if nsamples = 0, it won't match anything, so no need to do anything here for drop_instance_noupdate_no_writers */
    get_trigger_info_cmn (&post.c, inst);
    assert (trig_qc.dec_conds_invsample == 0);
    assert (trig_qc.dec_conds_sample == 0);
    assert (trig_qc.inc_conds_invsample == 0);
    assert (trig_qc.inc_conds_sample == 0);
    update_conditions_locked (rhc, false, &pre, &post, &trig_qc, inst);
  }

  if (inst_is_empty (inst))
    account_for_nonempty_to_empty_transition (rhc, instptr, "take: ");
  return n;
}

static int32_t read_w_qminv (struct dds_rhc_default * __restrict rhc, bool lock, void * __restrict * __restrict values, dds_sample_info_t * __restrict info_seq, int32_t max_samples, uint32_t qminv, dds_instance_handle_t handle, dds_readcond * __restrict cond, read_take_to_sample_t to_sample, read_take_to_invsample_t to_invsample)
{
  int32_t n = 0;
  assert (max_samples > 0);
  if (lock)
  {
    ddsrt_mutex_lock (&rhc->lock);
  }

  TRACE ("read_w_qminv(%p,%p,%p,%"PRId32",%"PRIx32",%"PRIx64",%p) - inst %"PRIu32" nonempty %"PRIu32" disp %"PRIu32" nowr %"PRIu32" new %"PRIu32" samples %"PRIu32"+%"PRIu32" read %"PRIu32"+%"PRIu32"\n",
    (void *) rhc, (void *) values, (void *) info_seq, max_samples, qminv, handle, (void *) cond,
    rhc->n_instances, rhc->n_nonempty_instances, rhc->n_not_alive_disposed,
    rhc->n_not_alive_no_writers, rhc->n_new, rhc->n_vsamples, rhc->n_invsamples,
    rhc->n_vread, rhc->n_invread);

  const dds_querycond_mask_t qcmask = (cond && cond->m_query.m_filter) ? cond->m_query.m_qcmask : 0;
  if (handle)
  {
    struct rhc_instance template, *inst;
    template.iid = handle;
    if ((inst = ddsrt_hh_lookup (rhc->instances, &template)) != NULL)
      n = read_w_qminv_inst (rhc, inst, values, info_seq, max_samples, qminv, qcmask, to_sample, to_invsample);
    else
      n = DDS_RETCODE_PRECONDITION_NOT_MET;
  }
  else if (!ddsrt_circlist_isempty (&rhc->nonempty_instances))
  {
    struct rhc_instance * inst = oldest_nonempty_instance (rhc);
    struct rhc_instance * const end = inst;
    do {
      n += read_w_qminv_inst(rhc, inst, values + n, info_seq + n, max_samples - n, qminv, qcmask, to_sample, to_invsample);
      inst = next_nonempty_instance (inst);
    } while (inst != end && n < max_samples);
  }
  TRACE ("read: returning %"PRIu32"\n", n);
  assert (rhc_check_counts_locked (rhc, true, false));

  // FIXME: conditional "lock" plus unconditional "unlock" is inexcusably bad design
  // It appears to have been introduced at some point so another language binding could lock
  // the RHC using dds_rhc_default_lock_samples to find out the number of samples present,
  // then allocate stuff and call read/take with lock=true. All that needs fixing.
  ddsrt_mutex_unlock (&rhc->lock);
  return n;
}

static int32_t take_w_qminv (struct dds_rhc_default * __restrict rhc, bool lock, void * __restrict * __restrict values, dds_sample_info_t * __restrict info_seq, int32_t max_samples, uint32_t qminv, dds_instance_handle_t handle, dds_readcond * __restrict cond, read_take_to_sample_t to_sample, read_take_to_invsample_t to_invsample)
{
  int32_t n = 0;
  assert (max_samples > 0);
  if (lock)
  {
    ddsrt_mutex_lock (&rhc->lock);
  }

  TRACE ("take_w_qminv(%p,%p,%p,%"PRId32",%"PRIx32",%"PRIx64",%p) - inst %"PRIu32" nonempty %"PRIu32" disp %"PRIu32" nowr %"PRIu32" new %"PRIu32" samples %"PRIu32"+%"PRIu32" read %"PRIu32"+%"PRIu32"\n",
    (void*) rhc, (void*) values, (void*) info_seq, max_samples, qminv, handle, (void *) cond,
    rhc->n_instances, rhc->n_nonempty_instances, rhc->n_not_alive_disposed,
    rhc->n_not_alive_no_writers, rhc->n_new, rhc->n_vsamples,
    rhc->n_invsamples, rhc->n_vread, rhc->n_invread);

  const dds_querycond_mask_t qcmask = (cond && cond->m_query.m_filter) ? cond->m_query.m_qcmask : 0;
  if (handle)
  {
    struct rhc_instance template, *inst;
    template.iid = handle;
    if ((inst = ddsrt_hh_lookup (rhc->instances, &template)) != NULL)
      n = take_w_qminv_inst (rhc, &inst, values, info_seq, max_samples, qminv, qcmask, to_sample, to_invsample);
    else
      n = DDS_RETCODE_PRECONDITION_NOT_MET;
  }
  else if (!ddsrt_circlist_isempty (&rhc->nonempty_instances))
  {
    struct rhc_instance *inst = oldest_nonempty_instance (rhc);
    uint32_t n_insts = rhc->n_nonempty_instances;
    while (n_insts-- > 0 && n < max_samples)
    {
      struct rhc_instance * const inst1 = next_nonempty_instance (inst);
      n += take_w_qminv_inst (rhc, &inst, values + n, info_seq + n, max_samples - n, qminv, qcmask, to_sample, to_invsample);
      inst = inst1;
    }
  }
  TRACE ("take: returning %"PRIu32"\n", n);
  assert (rhc_check_counts_locked (rhc, true, false));

  // FIXME: conditional "lock" plus unconditional "unlock" is inexcusably bad design
  // It appears to have been introduced at some point so another language binding could lock
  // the RHC using dds_rhc_default_lock_samples to find out the number of samples present,
  // then allocate stuff and call read/take with lock=true. All that needs fixing.
  ddsrt_mutex_unlock (&rhc->lock);
  return n;
}

static int32_t dds_rhc_read_w_qminv (struct dds_rhc_default *rhc, bool lock, void **values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t qminv, dds_instance_handle_t handle, dds_readcond *cond)
{
  assert (max_samples <= INT32_MAX);
  return read_w_qminv (rhc, lock, values, info_seq, (int32_t) max_samples, qminv, handle, cond, read_take_to_sample, read_take_to_invsample);
}

static int32_t dds_rhc_take_w_qminv (struct dds_rhc_default *rhc, bool lock, void **values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t qminv, dds_instance_handle_t handle, dds_readcond *cond)
{
  assert (max_samples <= INT32_MAX);
  return take_w_qminv (rhc, lock, values, info_seq, (int32_t) max_samples, qminv, handle, cond, read_take_to_sample, read_take_to_invsample);
}

static int32_t dds_rhc_readcdr_w_qminv (struct dds_rhc_default *rhc, bool lock, struct ddsi_serdata **values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t qminv, dds_instance_handle_t handle, dds_readcond *cond)
{
  DDSRT_STATIC_ASSERT (sizeof (void *) == sizeof (struct ddsi_serdata *));
  assert (max_samples <= INT32_MAX);
  return read_w_qminv (rhc, lock, (void **) values, info_seq, (int32_t) max_samples, qminv, handle, cond, read_take_to_sample_ref, read_take_to_invsample_ref);
}

static int32_t dds_rhc_takecdr_w_qminv (struct dds_rhc_default *rhc, bool lock, struct ddsi_serdata **values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t qminv, dds_instance_handle_t handle, dds_readcond *cond)
{
  DDSRT_STATIC_ASSERT (sizeof (void *) == sizeof (struct ddsi_serdata *));
  assert (max_samples <= INT32_MAX);
  return take_w_qminv (rhc, lock, (void **) values, info_seq, (int32_t) max_samples, qminv, handle, cond, read_take_to_sample_ref, read_take_to_invsample_ref);
}

/*************************
 ******   WAITSET   ******
 *************************/

static uint32_t rhc_get_cond_trigger (struct rhc_instance * const inst, const dds_readcond * const c)
{
  assert (!inst_is_empty (inst));
  bool m = ((qmask_of_inst (inst) & c->m_qminv) == 0);
  switch (c->m_sample_states)
  {
    case DDS_SST_READ:
      m = m && inst_has_read (inst);
      break;
    case DDS_SST_NOT_READ:
      m = m && inst_has_unread (inst);
      break;
    case DDS_SST_READ | DDS_SST_NOT_READ:
    case 0:
      /* note: we get here only if inst not empty, so this is a no-op */
      m = m && !inst_is_empty (inst);
      break;
    default:
      DDS_FATAL("update_readconditions: sample_states invalid: %"PRIx32"\n", c->m_sample_states);
  }
  return m ? 1 : 0;
}

static bool cond_is_sample_state_dependent (const struct dds_readcond *cond)
{
  switch (cond->m_sample_states)
  {
    case DDS_SST_READ:
    case DDS_SST_NOT_READ:
      return true;
    case DDS_SST_READ | DDS_SST_NOT_READ:
    case 0:
      return false;
    default:
      DDS_FATAL("update_readconditions: sample_states invalid: %"PRIx32"\n", cond->m_sample_states);
      return false;
  }
}

static bool dds_rhc_default_add_readcondition (struct dds_rhc *rhc_common, dds_readcond *cond)
{
  /* On the assumption that a readcondition will be attached to a
     waitset for nearly all of its life, we keep track of all
     readconditions on a reader in one set, without distinguishing
     between those attached to a waitset or not. */
  struct dds_rhc_default * const rhc = (struct dds_rhc_default *) rhc_common;
  struct ddsrt_hh_iter it;

  assert ((dds_entity_kind (&cond->m_entity) == DDS_KIND_COND_READ && cond->m_query.m_filter == 0) ||
          (dds_entity_kind (&cond->m_entity) == DDS_KIND_COND_QUERY && cond->m_query.m_filter != 0));
  assert (ddsrt_atomic_ld32 (&cond->m_entity.m_status.m_trigger) == 0);
  assert (cond->m_query.m_qcmask == 0);

  cond->m_qminv = qmask_from_dcpsquery (cond->m_sample_states, cond->m_view_states, cond->m_instance_states);

  ddsrt_mutex_lock (&rhc->lock);

  /* Allocate a slot in the condition bitmasks; return an error no more slots are available */
  if (cond->m_query.m_filter != 0)
  {
    dds_querycond_mask_t avail_qcmask = ~(dds_querycond_mask_t)0;
    for (dds_readcond *rc = rhc->conds; rc != NULL; rc = rc->m_next)
    {
      assert ((rc->m_query.m_filter == 0 && rc->m_query.m_qcmask == 0) || (rc->m_query.m_filter != 0 && rc->m_query.m_qcmask != 0));
      avail_qcmask &= ~rc->m_query.m_qcmask;
    }
    if (avail_qcmask == 0)
    {
      /* no available indices */
      ddsrt_mutex_unlock (&rhc->lock);
      return false;
    }

    /* use the least significant bit set */
    cond->m_query.m_qcmask = avail_qcmask & (~avail_qcmask + 1);
  }

  rhc->nconds++;
  cond->m_next = rhc->conds;
  rhc->conds = cond;

  uint32_t trigger = 0;
  if (cond->m_query.m_filter == 0)
  {
    /* Read condition is not cached inside the instances and samples, so it only needs
       to be evaluated on the non-empty instances */
    if (!ddsrt_circlist_isempty (&rhc->nonempty_instances))
    {
      struct rhc_instance *inst = latest_nonempty_instance (rhc);
      struct rhc_instance const * const end = inst;
      do {
        trigger += rhc_get_cond_trigger (inst, cond);
        inst = next_nonempty_instance (inst);
      } while (inst != end);
    }
  }
  else
  {
    if (cond_is_sample_state_dependent (cond))
      rhc->qconds_samplest |= cond->m_query.m_qcmask;
    if (rhc->nqconds++ == 0)
    {
      assert (rhc->qcond_eval_samplebuf == NULL);
      rhc->qcond_eval_samplebuf = ddsi_sertype_alloc_sample (rhc->type);
    }

    /* Attaching a query condition means clearing the allocated bit in all instances and
       samples, except for those that match the predicate. */
    const dds_querycond_mask_t qcmask = cond->m_query.m_qcmask;
    for (struct rhc_instance *inst = ddsrt_hh_iter_first (rhc->instances, &it); inst != NULL; inst = ddsrt_hh_iter_next (&it))
    {
      const bool instmatch = eval_predicate_invsample (rhc, inst, cond->m_query.m_filter);;
      uint32_t matches = 0;

      inst->conds = (inst->conds & ~qcmask) | (instmatch ? qcmask : 0);
      if (inst->latest)
      {
        struct rhc_sample *sample = inst->latest->next, * const end = sample;
        do {
          const bool m = eval_predicate_sample (rhc, sample->sample, cond->m_query.m_filter);
          sample->conds = (sample->conds & ~qcmask) | (m ? qcmask : 0);
          matches += m;
          sample = sample->next;
        } while (sample != end);
      }

      if (!inst_is_empty (inst) && rhc_get_cond_trigger (inst, cond))
        trigger += (inst->inv_exists ? instmatch : 0) + matches;
    }
  }

  if (trigger)
  {
    ddsrt_atomic_st32 (&cond->m_entity.m_status.m_trigger, trigger);
    dds_entity_status_signal (&cond->m_entity, DDS_DATA_AVAILABLE_STATUS);
  }

  TRACE ("add_readcondition(%p, %"PRIx32", %"PRIx32", %"PRIx32") => %p qminv %"PRIx32" ; rhc %"PRIu32" conds\n",
    (void *) rhc, cond->m_sample_states, cond->m_view_states,
    cond->m_instance_states, (void *) cond, cond->m_qminv, rhc->nconds);

  ddsrt_mutex_unlock (&rhc->lock);
  return true;
}

static void dds_rhc_default_remove_readcondition (struct dds_rhc *rhc_common, dds_readcond *cond)
{
  struct dds_rhc_default * const rhc = (struct dds_rhc_default *) rhc_common;
  dds_readcond **ptr;
  ddsrt_mutex_lock (&rhc->lock);
  ptr = &rhc->conds;
  while (*ptr != cond)
    ptr = &(*ptr)->m_next;
  *ptr = (*ptr)->m_next;
  rhc->nconds--;
  if (cond->m_query.m_filter)
  {
    rhc->nqconds--;
    rhc->qconds_samplest &= ~cond->m_query.m_qcmask;
    cond->m_query.m_qcmask = 0;
    if (rhc->nqconds == 0)
    {
      assert (rhc->qcond_eval_samplebuf != NULL);
      ddsi_sertype_free_sample (rhc->type, rhc->qcond_eval_samplebuf, DDS_FREE_ALL);
      rhc->qcond_eval_samplebuf = NULL;
    }
  }
  ddsrt_mutex_unlock (&rhc->lock);
}

static bool update_conditions_locked (struct dds_rhc_default *rhc, bool called_from_insert, const struct trigger_info_pre *pre, const struct trigger_info_post *post, const struct trigger_info_qcond *trig_qc, const struct rhc_instance *inst)
{
  /* Pre: rhc->lock held; returns 1 if triggering required, else 0. */
  bool trigger = false;
  dds_readcond *iter;
  bool m_pre, m_post;

  TRACE ("update_conditions_locked(%p %p) - inst %"PRIu32" nonempty %"PRIu32" disp %"PRIu32" nowr %"PRIu32" new %"PRIu32" samples %"PRIu32" read %"PRIu32"\n",
         (void *) rhc, (void *) inst, rhc->n_instances, rhc->n_nonempty_instances, rhc->n_not_alive_disposed,
         rhc->n_not_alive_no_writers, rhc->n_new, rhc->n_vsamples, rhc->n_vread);
  TRACE ("  pre (%"PRIx32",%d,%d) post (%"PRIx32",%d,%d) read -[%d,%d]+[%d,%d] qcmask -[%"PRIx32",%"PRIx32"]+[%"PRIx32",%"PRIx32"]\n",
         pre->c.qminst, pre->c.has_read, pre->c.has_not_read,
         post->c.qminst, post->c.has_read, post->c.has_not_read,
         trig_qc->dec_invsample_read, trig_qc->dec_sample_read, trig_qc->inc_invsample_read, trig_qc->inc_sample_read,
         trig_qc->dec_conds_invsample, trig_qc->dec_conds_sample, trig_qc->inc_conds_invsample, trig_qc->inc_conds_sample);

  assert (rhc->n_nonempty_instances >= rhc->n_not_alive_disposed + rhc->n_not_alive_no_writers);
#ifndef DDS_HAS_LIFESPAN
  /* If lifespan is disabled, samples cannot expire and therefore
     empty instances cannot be in the 'new' state. */
  assert (rhc->n_nonempty_instances >= rhc->n_new);
#endif
  assert (rhc->n_vsamples >= rhc->n_vread);

  iter = rhc->conds;
  while (iter)
  {
    m_pre = ((pre->c.qminst & iter->m_qminv) == 0);
    m_post = ((post->c.qminst & iter->m_qminv) == 0);

    /* Fast path out: instance did not and will not match based on instance, view states, so no
       need to evaluate anything else */
    if (!m_pre && !m_post)
    {
      iter = iter->m_next;
      continue;
    }

    /* FIXME: use bitmask? */
    switch (iter->m_sample_states)
    {
      case DDS_SST_READ:
        m_pre = m_pre && pre->c.has_read;
        m_post = m_post && post->c.has_read;
        break;
      case DDS_SST_NOT_READ:
        m_pre = m_pre && pre->c.has_not_read;
        m_post = m_post && post->c.has_not_read;
        break;
      case DDS_SST_READ | DDS_SST_NOT_READ:
      case 0:
        m_pre = m_pre && (pre->c.has_read + pre->c.has_not_read);
        m_post = m_post && (post->c.has_read + post->c.has_not_read);
        break;
      default:
        DDS_FATAL ("update_readconditions: sample_states invalid: %"PRIx32"\n", iter->m_sample_states);
    }

    TRACE ("  cond %p %08"PRIx32": ", (void *) iter, iter->m_query.m_qcmask);
    if (iter->m_query.m_filter == 0)
    {
      assert (dds_entity_kind (&iter->m_entity) == DDS_KIND_COND_READ);
      if (m_pre == m_post)
        TRACE ("no change");
      else if (m_pre < m_post)
      {
        TRACE ("now matches");
        trigger = (ddsrt_atomic_inc32_ov (&iter->m_entity.m_status.m_trigger) == 0);
        if (trigger)
          TRACE (" (cond now triggers)");
      }
      else
      {
        TRACE ("no longer matches");
        if (ddsrt_atomic_dec32_nv (&iter->m_entity.m_status.m_trigger) == 0)
          TRACE (" (cond no longer triggers)");
      }
    }
    else if (m_pre || m_post) /* no need to look any further if both are false */
    {
      assert (dds_entity_kind (&iter->m_entity) == DDS_KIND_COND_QUERY);
      assert (iter->m_query.m_qcmask != 0);
      const dds_querycond_mask_t qcmask = iter->m_query.m_qcmask;
      int32_t mdelta = 0;

      switch (iter->m_sample_states)
      {
        case DDS_SST_READ:
          if (trig_qc->dec_invsample_read)
            mdelta -= (trig_qc->dec_conds_invsample & qcmask) != 0;
          if (trig_qc->dec_sample_read)
            mdelta -= (trig_qc->dec_conds_sample & qcmask) != 0;
          if (trig_qc->inc_invsample_read)
            mdelta += (trig_qc->inc_conds_invsample & qcmask) != 0;
          if (trig_qc->inc_sample_read)
            mdelta += (trig_qc->inc_conds_sample & qcmask) != 0;
          break;
        case DDS_SST_NOT_READ:
          if (!trig_qc->dec_invsample_read)
            mdelta -= (trig_qc->dec_conds_invsample & qcmask) != 0;
          if (!trig_qc->dec_sample_read)
            mdelta -= (trig_qc->dec_conds_sample & qcmask) != 0;
          if (!trig_qc->inc_invsample_read)
            mdelta += (trig_qc->inc_conds_invsample & qcmask) != 0;
          if (!trig_qc->inc_sample_read)
            mdelta += (trig_qc->inc_conds_sample & qcmask) != 0;
          break;
        case DDS_SST_READ | DDS_SST_NOT_READ:
        case 0:
          mdelta -= (trig_qc->dec_conds_invsample & qcmask) != 0;
          mdelta -= (trig_qc->dec_conds_sample & qcmask) != 0;
          mdelta += (trig_qc->inc_conds_invsample & qcmask) != 0;
          mdelta += (trig_qc->inc_conds_sample & qcmask) != 0;
          break;
        default:
          DDS_FATAL ("update_readconditions: sample_states invalid: %"PRIx32"\n", iter->m_sample_states);
      }

      if (m_pre == m_post)
      {
        assert (m_pre);
        /* there was a match at read-condition level
           - therefore the matching samples in the instance are accounted for in the trigger count
           - therefore an incremental update is required
           there is always space for a valid and an invalid sample, both add and remove
           inserting an update always has unread data added, but a read pretends it is a removal
           of whatever and an insertion of read data */
        assert (mdelta >= 0 || ddsrt_atomic_ld32 (&iter->m_entity.m_status.m_trigger) >= (uint32_t) -mdelta);
        if (mdelta == 0)
          TRACE ("no change @ %"PRIu32" (0)", ddsrt_atomic_ld32 (&iter->m_entity.m_status.m_trigger));
        else
          TRACE ("m=%"PRId32" @ %"PRIu32" (0)", mdelta, ddsrt_atomic_ld32 (&iter->m_entity.m_status.m_trigger) + (uint32_t) mdelta);
        /* even though it matches now and matched before, it is not a given that any of the samples
           matched before, so m_trigger may still be 0 */
        const uint32_t ov = ddsrt_atomic_add32_ov (&iter->m_entity.m_status.m_trigger, (uint32_t) mdelta);
        if (mdelta > 0 && ov == 0)
          trigger = true;
        if (trigger)
          TRACE (" (cond now triggers)");
        else if (mdelta < 0 && ov == (uint32_t) -mdelta)
          TRACE (" (cond no longer triggers)");
      }
      else
      {
        /* There either was no match at read-condition level, now there is: scan all samples for matches;
           or there was a match and now there is not: so also scan all samples for matches.  The only
           difference is in whether the number of matches should be added or subtracted. */
        int32_t mcurrent = 0;
        if (inst)
        {
          if (inst->inv_exists)
            mcurrent += (qmask_of_invsample (inst) & iter->m_qminv) == 0 && (inst->conds & qcmask) != 0;
          if (inst->latest)
          {
            struct rhc_sample *sample = inst->latest->next, * const end = sample;
            do {
              mcurrent += (qmask_of_sample (sample) & iter->m_qminv) == 0 && (sample->conds & qcmask) != 0;
              sample = sample->next;
            } while (sample != end);
          }
        }
        if (mdelta == 0 && mcurrent == 0)
          TRACE ("no change @ %"PRIu32" (2)", ddsrt_atomic_ld32 (&iter->m_entity.m_status.m_trigger));
        else if (m_pre < m_post)
        {
          /* No match previously, so the instance wasn't accounted for at all in the trigger value.
             Therefore when inserting data, all that matters is how many currently match.

             When reading or taking it is evaluated incrementally _before_ changing the state of the
             sample, so mrem reflects the state before the change, and the incremental change needs
             to be taken into account. */
          const int32_t m = called_from_insert ? mcurrent : mcurrent + mdelta;
          TRACE ("mdelta=%"PRId32" mcurrent=%"PRId32" => %"PRId32" => %"PRIu32" (2a)", mdelta, mcurrent, m, ddsrt_atomic_ld32 (&iter->m_entity.m_status.m_trigger) + (uint32_t) m);
          assert (m >= 0 || ddsrt_atomic_ld32 (&iter->m_entity.m_status.m_trigger) >= (uint32_t) -m);
          trigger = (ddsrt_atomic_add32_ov (&iter->m_entity.m_status.m_trigger, (uint32_t) m) == 0 && m > 0);
          if (trigger)
            TRACE (" (cond now triggers)");
        }
        else
        {
          /* Previously matched, but no longer, which means we need to subtract the current number
             of matches as well as those that were removed just before, hence need the incremental
             change as well */
          const int32_t m = mcurrent - mdelta;
          TRACE ("mdelta=%"PRId32" mcurrent=%"PRId32" => %"PRId32" => %"PRIu32" (2b)", mdelta, mcurrent, m, ddsrt_atomic_ld32 (&iter->m_entity.m_status.m_trigger) - (uint32_t) m);
          assert (m < 0 || ddsrt_atomic_ld32 (&iter->m_entity.m_status.m_trigger) >= (uint32_t) m);
          if (ddsrt_atomic_sub32_nv (&iter->m_entity.m_status.m_trigger, (uint32_t) m) == 0)
            TRACE (" (cond no longer triggers)");
        }
      }
    }

    if (trigger)
    {
      dds_entity_status_signal (&iter->m_entity, DDS_DATA_AVAILABLE_STATUS);
    }
    TRACE ("\n");
    iter = iter->m_next;
  }
  return trigger;
}


/*************************
 ******  READ/TAKE  ******
 *************************/

static int32_t dds_rhc_default_read (struct dds_rhc *rhc_common, bool lock, void **values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t mask, dds_instance_handle_t handle, dds_readcond *cond)
{
  struct dds_rhc_default * const rhc = (struct dds_rhc_default *) rhc_common;
  uint32_t qminv = qmask_from_mask_n_cond (mask, cond);
  return dds_rhc_read_w_qminv (rhc, lock, values, info_seq, max_samples, qminv, handle, cond);
}

static int32_t dds_rhc_default_take (struct dds_rhc *rhc_common, bool lock, void **values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t mask, dds_instance_handle_t handle, dds_readcond *cond)
{
  struct dds_rhc_default * const rhc = (struct dds_rhc_default *) rhc_common;
  uint32_t qminv = qmask_from_mask_n_cond(mask, cond);
  return dds_rhc_take_w_qminv (rhc, lock, values, info_seq, max_samples, qminv, handle, cond);
}

static int32_t dds_rhc_default_readcdr (struct dds_rhc *rhc_common, bool lock, struct ddsi_serdata ** values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t sample_states, uint32_t view_states, uint32_t instance_states, dds_instance_handle_t handle)
{
  struct dds_rhc_default * const rhc = (struct dds_rhc_default *) rhc_common;
  uint32_t qminv = qmask_from_dcpsquery (sample_states, view_states, instance_states);
  return dds_rhc_readcdr_w_qminv (rhc, lock, values, info_seq, max_samples, qminv, handle, NULL);
}

static int32_t dds_rhc_default_takecdr (struct dds_rhc *rhc_common, bool lock, struct ddsi_serdata ** values, dds_sample_info_t *info_seq, uint32_t max_samples, uint32_t sample_states, uint32_t view_states, uint32_t instance_states, dds_instance_handle_t handle)
{
  struct dds_rhc_default * const rhc = (struct dds_rhc_default *) rhc_common;
  uint32_t qminv = qmask_from_dcpsquery (sample_states, view_states, instance_states);
  return dds_rhc_takecdr_w_qminv (rhc, lock, values, info_seq, max_samples, qminv, handle, NULL);
}

/*************************
 ******    CHECK    ******
 *************************/

#ifndef NDEBUG
#define CHECK_MAX_CONDS 64
static int rhc_check_counts_locked (struct dds_rhc_default *rhc, bool check_conds, bool check_qcmask)
{
  if (!rhc->xchecks)
    return 1;

  const uint32_t ncheck = rhc->nconds < CHECK_MAX_CONDS ? rhc->nconds : CHECK_MAX_CONDS;
  uint32_t n_instances = 0, n_nonempty_instances = 0;
  uint32_t n_not_alive_disposed = 0, n_not_alive_no_writers = 0, n_new = 0;
  uint32_t n_vsamples = 0, n_vread = 0;
  uint32_t n_invsamples = 0, n_invread = 0;
  uint32_t cond_match_count[CHECK_MAX_CONDS];
  dds_querycond_mask_t enabled_qcmask = 0;
  struct rhc_instance *inst;
  struct ddsrt_hh_iter iter;
  dds_readcond *rciter;
  uint32_t i;

  for (i = 0; i < CHECK_MAX_CONDS; i++)
    cond_match_count[i] = 0;

  for (rciter = rhc->conds; rciter; rciter = rciter->m_next)
  {
    assert ((dds_entity_kind (&rciter->m_entity) == DDS_KIND_COND_READ && rciter->m_query.m_filter == 0) ||
            (dds_entity_kind (&rciter->m_entity) == DDS_KIND_COND_QUERY && rciter->m_query.m_filter != 0));
    assert ((rciter->m_query.m_filter != 0) == (rciter->m_query.m_qcmask != 0));
    assert (!(enabled_qcmask & rciter->m_query.m_qcmask));
    enabled_qcmask |= rciter->m_query.m_qcmask;
  }

  for (inst = ddsrt_hh_iter_first (rhc->instances, &iter); inst; inst = ddsrt_hh_iter_next (&iter))
  {
    uint32_t n_vsamples_in_instance = 0, n_read_vsamples_in_instance = 0;
    bool a_sample_free = true;

    n_instances++;
    if (inst->isnew)
      n_new++;
    if (inst_is_empty (inst))
      continue;

    n_nonempty_instances++;
    if (inst->isdisposed)
      n_not_alive_disposed++;
    else if (inst->wrcount == 0)
      n_not_alive_no_writers++;

    if (inst->latest)
    {
      struct rhc_sample *sample = inst->latest->next, * const end = sample;
      do {
        if (sample == &inst->a_sample)
        {
          assert (a_sample_free);
          a_sample_free = false;
        }
        n_vsamples++;
        n_vsamples_in_instance++;
        if (sample->isread)
        {
          n_vread++;
          n_read_vsamples_in_instance++;
        }
        sample = sample->next;
      } while (sample != end);
    }

    if (inst->inv_exists)
    {
      n_invsamples++;
      n_invread += inst->inv_isread;
    }

    assert (n_read_vsamples_in_instance == inst->nvread);
    assert (n_vsamples_in_instance == inst->nvsamples);
    assert (a_sample_free == inst->a_sample_free);

    if (check_conds)
    {
      if (check_qcmask && rhc->nqconds > 0)
      {
        dds_querycond_mask_t qcmask;
        untyped_to_clean_invsample (rhc->type, inst->tk->m_sample, rhc->qcond_eval_samplebuf, 0, 0);
        qcmask = 0;
        for (rciter = rhc->conds; rciter; rciter = rciter->m_next)
          if (rciter->m_query.m_filter != 0 && rciter->m_query.m_filter (rhc->qcond_eval_samplebuf))
            qcmask |= rciter->m_query.m_qcmask;
        assert ((inst->conds & enabled_qcmask) == qcmask);
        if (inst->latest)
        {
          struct rhc_sample *sample = inst->latest->next, * const end = sample;
          do {
            ddsi_serdata_to_sample (sample->sample, rhc->qcond_eval_samplebuf, NULL, NULL);
            qcmask = 0;
            for (rciter = rhc->conds; rciter; rciter = rciter->m_next)
              if (rciter->m_query.m_filter != 0 && rciter->m_query.m_filter (rhc->qcond_eval_samplebuf))
                qcmask |= rciter->m_query.m_qcmask;
            assert ((sample->conds & enabled_qcmask) == qcmask);
            sample = sample->next;
          } while (sample != end);
        }
      }

      for (i = 0, rciter = rhc->conds; rciter && i < ncheck; i++, rciter = rciter->m_next)
      {
        if (!rhc_get_cond_trigger (inst, rciter))
          ;
        else if (rciter->m_query.m_filter == 0)
          cond_match_count[i]++;
        else
        {
          if (inst->inv_exists)
            cond_match_count[i] += (qmask_of_invsample (inst) & rciter->m_qminv) == 0 && (inst->conds & rciter->m_query.m_qcmask) != 0;
          if (inst->latest)
          {
            struct rhc_sample *sample = inst->latest->next, * const end = sample;
            do {
              cond_match_count[i] += ((qmask_of_sample (sample) & rciter->m_qminv) == 0 && (sample->conds & rciter->m_query.m_qcmask) != 0);
              sample = sample->next;
            } while (sample != end);
          }
        }
      }
    }
  }

  assert (rhc->n_instances == n_instances);
  assert (rhc->n_nonempty_instances == n_nonempty_instances);
  assert (rhc->n_not_alive_disposed == n_not_alive_disposed);
  assert (rhc->n_not_alive_no_writers == n_not_alive_no_writers);
  assert (rhc->n_new == n_new);
  assert (rhc->n_vsamples == n_vsamples);
  assert (rhc->n_vread == n_vread);
  assert (rhc->n_invsamples == n_invsamples);
  assert (rhc->n_invread == n_invread);

  if (check_conds)
  {
    for (i = 0, rciter = rhc->conds; rciter && i < ncheck; i++, rciter = rciter->m_next)
      assert (cond_match_count[i] == ddsrt_atomic_ld32 (&rciter->m_entity.m_status.m_trigger));
  }

  if (rhc->n_nonempty_instances == 0)
  {
    assert (ddsrt_circlist_isempty (&rhc->nonempty_instances));
  }
  else
  {
    assert (!ddsrt_circlist_isempty (&rhc->nonempty_instances));
    struct ddsrt_circlist_elem const *prev = rhc->nonempty_instances.latest->prev;
    inst = latest_nonempty_instance (rhc);
    struct rhc_instance const * const end = inst;
    n_nonempty_instances = 0;
    do {
      assert (!inst_is_empty (inst));
      assert (prev->next == &inst->nonempty_list);
      assert (inst->nonempty_list.prev == prev);
      prev = &inst->nonempty_list;
      inst = next_nonempty_instance (inst);
      n_nonempty_instances++;
    } while (inst != end);
    assert (rhc->n_nonempty_instances == n_nonempty_instances);
  }

  return 1;
}
#undef CHECK_MAX_CONDS
#endif

static const struct dds_rhc_ops dds_rhc_default_ops = {
  .rhc_ops = {
    .store = dds_rhc_default_store,
    .unregister_wr = dds_rhc_default_unregister_wr,
    .relinquish_ownership = dds_rhc_default_relinquish_ownership,
    .set_qos = dds_rhc_default_set_qos,
    .free = dds_rhc_default_free
  },
  .read = dds_rhc_default_read,
  .take = dds_rhc_default_take,
  .readcdr = dds_rhc_default_readcdr,
  .takecdr = dds_rhc_default_takecdr,
  .add_readcondition = dds_rhc_default_add_readcondition,
  .remove_readcondition = dds_rhc_default_remove_readcondition,
  .lock_samples = dds_rhc_default_lock_samples,
  .associate = dds_rhc_default_associate
};
