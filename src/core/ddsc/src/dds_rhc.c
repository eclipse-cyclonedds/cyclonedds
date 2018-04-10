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
#include <assert.h>
#include <string.h>

#if HAVE_VALGRIND && ! defined (NDEBUG)
#include <memcheck.h>
#define USE_VALGRIND 1
#else
#define USE_VALGRIND 0
#endif

#include "os/os.h"

#include "dds__entity.h"
#include "dds__reader.h"
#include "dds__rhc.h"
#include "dds__tkmap.h"
#include "util/ut_hopscotch.h"

#include "util/ut_avl.h"
#include "ddsi/q_xqos.h"
#include "ddsi/q_error.h"
#include "ddsi/q_unused.h"
#include "q__osplser.h"
#include "ddsi/q_config.h"
#include "ddsi/q_globals.h"
#include "ddsi/q_radmin.h" /* sampleinfo */
#include "ddsi/q_entity.h" /* proxy_writer_info */
#include "ddsi/sysdeps.h"
#include "dds__report.h"

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

   Lifespan, time base filter and deadline, are based on the instance
   timestamp ("tstamp").  This time stamp needs to be changed to either source
   or reception timestamp, depending on the ordering chosen.

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
   The actual signalling of the waitsets then takes places later, by calling
   "signal_conditions" after releasing the RHC lock.
*/

static const status_cb_data_t dds_rhc_data_avail_cb_data = { DDS_DATA_AVAILABLE_STATUS, 0, 0, true };

/* FIXME: populate tkmap with key-only derived serdata, with timestamp
   set to invalid.  An invalid timestamp is (logically) unordered with
   respect to valid timestamps, and that would mean BY_SOURCE order
   would be respected even when generating an invalid sample for an
   unregister message using the tkmap data. */

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
  struct ut_ehh * regs;
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
  rt->regs = ut_ehhNew (sizeof (struct lwreg), 1, lwreg_hash, lwreg_equals);
}

static void lwregs_fini (struct lwregs *rt)
{
  ut_ehhFree (rt->regs);
}

static int lwregs_contains (struct lwregs *rt, uint64_t iid, uint64_t wr_iid)
{
  struct lwreg dummy = { .iid = iid, .wr_iid = wr_iid };
  return ut_ehhLookup (rt->regs, &dummy) != NULL;
}

static int lwregs_add (struct lwregs *rt, uint64_t iid, uint64_t wr_iid)
{
  struct lwreg dummy = { .iid = iid, .wr_iid = wr_iid };
  return ut_ehhAdd (rt->regs, &dummy);
}

static int lwregs_delete (struct lwregs *rt, uint64_t iid, uint64_t wr_iid)
{
  struct lwreg dummy = { .iid = iid, .wr_iid = wr_iid };
  return ut_ehhRemove (rt->regs, &dummy);
}

/*************************
 ******     RHC     ******
 *************************/

struct rhc_sample
{
  struct serdata *sample;      /* serialised data (either just_key or real data) */
  struct rhc_sample *next;     /* next sample in time ordering, or oldest sample if most recent */
  uint64_t wr_iid;             /* unique id for writer of this sample (perhaps better in serdata) */
  nn_wctime_t rtstamp;         /* reception timestamp (not really required; perhaps better in serdata) */
  bool isread;                 /* READ or NOT_READ sample state */
  unsigned disposed_gen;       /* snapshot of instance counter at time of insertion */
  unsigned no_writers_gen;     /* __/ */
};

struct rhc_instance
{
  uint64_t iid;                /* unique instance id, key of table, also serves as instance handle */
  uint64_t wr_iid;             /* unique of id of writer of latest sample or 0 */
  struct rhc_sample *latest;   /* latest received sample; circular list old->new; null if no sample */
  unsigned nvsamples;          /* number of "valid" samples in instance */
  unsigned nvread;             /* number of READ "valid" samples in instance (0 <= nvread <= nvsamples) */
  uint32_t wrcount;            /* number of live writers */
  bool isnew;                  /* NEW or NOT_NEW view state */
  bool a_sample_free;          /* whether or not a_sample is in use */
  bool isdisposed;             /* DISPOSED or NOT_DISPOSED (if not disposed, wrcount determines ALIVE/NOT_ALIVE_NO_WRITERS) */
  bool has_changed;            /* To track changes in an instance - if number of samples are added or data is overwritten */
  unsigned inv_exists : 1;     /* whether or not state change occurred since last sample (i.e., must return invalid sample) */
  unsigned inv_isread : 1;     /* whether or not that state change has been read before */
  unsigned disposed_gen;       /* bloody generation counters - worst invention of mankind */
  unsigned no_writers_gen;     /* __/ */
  uint32_t strength;           /* "current" ownership strength */
  nn_guid_t wr_guid;           /* guid of last writer (if wr_iid != 0 then wr_guid is the corresponding guid, else undef) */
  nn_wctime_t tstamp;          /* source time stamp of last update */
  struct rhc_instance *next;   /* next non-empty instance in arbitrary ordering */
  struct rhc_instance *prev;
  struct tkmap_instance *tk;   /* backref into TK for unref'ing */
  struct rhc_sample a_sample;  /* pre-allocated storage for 1 sample */
};

typedef enum rhc_store_result
{
  RHC_STORED,
  RHC_FILTERED,
  RHC_REJECTED
}
rhc_store_result_t;

struct rhc
{
  struct ut_hh *instances;
  struct rhc_instance *nonempty_instances; /* circular, points to most recently added one, NULL if none */
  struct lwregs registrations;      /* should be a global one (with lock-free lookups) */

  /* Instance/Sample maximums from resource limits QoS */

  os_atomic_uint32_t n_cbs;                /* # callbacks in progress */
  int32_t max_instances; /* FIXME: probably better as uint32_t with MAX_UINT32 for unlimited */
  int32_t max_samples;   /* FIXME: probably better as uint32_t with MAX_UINT32 for unlimited */
  int32_t max_samples_per_instance; /* FIXME: probably better as uint32_t with MAX_UINT32 for unlimited */

  uint32_t n_instances;             /* # instances, including empty [NOT USED] */
  uint32_t n_nonempty_instances;    /* # non-empty instances */
  uint32_t n_not_alive_disposed;    /* # disposed, non-empty instances [NOT USED] */
  uint32_t n_not_alive_no_writers;  /* # not-alive-no-writers, non-empty instances [NOT USED] */
  uint32_t n_new;                   /* # new, non-empty instances [NOT USED] */
  uint32_t n_vsamples;              /* # "valid" samples over all instances */
  uint32_t n_vread;                 /* # read "valid" samples over all instances [NOT USED] */
  uint32_t n_invsamples;            /* # invalid samples over all instances [NOT USED] */
  uint32_t n_invread;               /* # read invalid samples over all instances [NOT USED] */

  bool by_source_ordering;          /* true if BY_SOURCE, false if BY_RECEPTION */
  bool exclusive_ownership;         /* true if EXCLUSIVE, false if SHARED */
  bool reliable;                    /* true if reliability RELIABLE */

  dds_reader * reader;              /* reader */
  const struct sertopic * topic;    /* topic description */
  unsigned history_depth;           /* depth, 1 for KEEP_LAST_1, 2**32-1 for KEEP_ALL */

  os_mutex lock;
  os_mutex conds_lock;
  dds_readcond * conds;             /* List of associated read conditions */
  uint32_t nconds;                  /* Number of associated read conditions */
};

struct trigger_info
{
  unsigned qminst;
  bool has_read;
  bool has_not_read;
  bool has_changed;
};

#define QMASK_OF_SAMPLE(s) ((s)->isread ? DDS_READ_SAMPLE_STATE : DDS_NOT_READ_SAMPLE_STATE)
#define QMASK_OF_INVSAMPLE(i) ((i)->inv_isread ? DDS_READ_SAMPLE_STATE : DDS_NOT_READ_SAMPLE_STATE)
#define INST_NSAMPLES(i) ((i)->nvsamples + (i)->inv_exists)
#define INST_NREAD(i) ((i)->nvread + ((i)->inv_exists & (i)->inv_isread))
#define INST_IS_EMPTY(i) (INST_NSAMPLES (i) == 0)
#define INST_HAS_READ(i) (INST_NREAD (i) > 0)
#define INST_HAS_UNREAD(i) (INST_NREAD (i) < INST_NSAMPLES (i))

static unsigned qmask_of_inst (const struct rhc_instance *inst);
static bool update_conditions_locked
(struct rhc *rhc, const struct trigger_info *pre, const struct trigger_info *post, const struct serdata *sample);
static void signal_conditions (struct rhc *rhc);
#ifndef NDEBUG
static int rhc_check_counts_locked (struct rhc *rhc, bool check_conds);
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

static void add_inst_to_nonempty_list (_Inout_ struct rhc *rhc, _Inout_ struct rhc_instance *inst)
{
  if (rhc->nonempty_instances == NULL)
  {
    inst->next = inst->prev = inst;
  }
  else
  {
    struct rhc_instance * const hd = rhc->nonempty_instances;
#ifndef NDEBUG
    {
      const struct rhc_instance *x = hd;
      do { assert (x != inst); x = x->next; } while (x != hd);
    }
#endif
    inst->next = hd->next;
    inst->prev = hd;
    hd->next = inst;
    inst->next->prev = inst;
  }
  rhc->nonempty_instances = inst;
  rhc->n_nonempty_instances++;
}

static void remove_inst_from_nonempty_list (struct rhc *rhc, struct rhc_instance *inst)
{
  assert (INST_IS_EMPTY (inst));
#ifndef NDEBUG
  {
    const struct rhc_instance *x = rhc->nonempty_instances;
    do { if (x == inst) break; x = x->next; } while (x != rhc->nonempty_instances);
    assert (x == inst);
  }
#endif

  if (inst->next == inst)
  {
    rhc->nonempty_instances = NULL;
  }
  else
  {
    struct rhc_instance * const inst_prev = inst->prev;
    struct rhc_instance * const inst_next = inst->next;
    inst_prev->next = inst_next;
    inst_next->prev = inst_prev;
    if (rhc->nonempty_instances == inst)
      rhc->nonempty_instances = inst_prev;
  }
  assert (rhc->n_nonempty_instances > 0);
  rhc->n_nonempty_instances--;
}

struct rhc * dds_rhc_new (dds_reader * reader, const struct sertopic * topic)
{
  struct rhc * rhc = dds_alloc (sizeof (*rhc));

  lwregs_init (&rhc->registrations);
  os_mutexInit (&rhc->lock);
  os_mutexInit (&rhc->conds_lock);
  rhc->instances = ut_hhNew (1, instance_iid_hash, instance_iid_eq);
  rhc->topic = topic;
  rhc->reader = reader;

  return rhc;
}

void dds_rhc_set_qos (struct rhc * rhc, const nn_xqos_t * qos)
{
  /* Set read related QoS */

  rhc->max_samples = qos->resource_limits.max_samples;
  rhc->max_instances = qos->resource_limits.max_instances;
  rhc->max_samples_per_instance = qos->resource_limits.max_samples_per_instance;
  rhc->by_source_ordering = (qos->destination_order.kind == NN_BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS);
  rhc->exclusive_ownership = (qos->ownership.kind == NN_EXCLUSIVE_OWNERSHIP_QOS);
  rhc->reliable = (qos->reliability.kind == NN_RELIABLE_RELIABILITY_QOS);
  rhc->history_depth = (qos->history.kind == NN_KEEP_LAST_HISTORY_QOS) ? qos->history.depth : ~0u;
}

static struct rhc_sample * alloc_sample (struct rhc_instance *inst)
{
  if (inst->a_sample_free)
  {
    inst->a_sample_free = false;
#if USE_VALGRIND
    VALGRIND_MAKE_MEM_UNDEFINED (&inst->a_sample, sizeof (inst->a_sample));
#endif
    return &inst->a_sample;
  }
  else
  {
    /* This instead of sizeof(rhc_sample) gets us type checking */
    struct rhc_sample *s;
    s = dds_alloc (sizeof (*s));
    return s;
  }
}

static void free_sample (struct rhc_instance *inst, struct rhc_sample *s)
{
  ddsi_serdata_unref (s->sample);
  if (s == &inst->a_sample)
  {
    assert (!inst->a_sample_free);
#if USE_VALGRIND
    VALGRIND_MAKE_MEM_NOACCESS (&inst->a_sample, sizeof (inst->a_sample));
#endif
    inst->a_sample_free = true;
  }
  else
  {
    dds_free (s);
  }
}

static void inst_clear_invsample (struct rhc *rhc, struct rhc_instance *inst)
{
  assert (inst->inv_exists);
  inst->inv_exists = 0;
  if (inst->inv_isread)
  {
    rhc->n_invread--;
  }
  rhc->n_invsamples--;
}

static void inst_clear_invsample_if_exists (struct rhc *rhc, struct rhc_instance *inst)
{
  if (inst->inv_exists)
  {
    inst_clear_invsample (rhc, inst);
  }
}

static void inst_set_invsample (struct rhc *rhc, struct rhc_instance *inst)
{
  /* Obviously optimisable, but that is perhaps not worth the bother */
  inst_clear_invsample_if_exists (rhc, inst);
  inst->inv_exists = 1;
  inst->inv_isread = 0;
  rhc->n_invsamples++;
}

static void free_instance (void *vnode, void *varg)
{
  struct rhc *rhc = varg;
  struct rhc_instance *inst = vnode;
  struct rhc_sample *s = inst->latest;
  const bool was_empty = INST_IS_EMPTY (inst);
  if (s)
  {
    do {
      struct rhc_sample * const s1 = s->next;
      free_sample (inst, s);
      s = s1;
    } while (s != inst->latest);
    rhc->n_vsamples -= inst->nvsamples;
    rhc->n_vread -= inst->nvread;
    inst->nvsamples = 0;
    inst->nvread = 0;
  }
  inst_clear_invsample_if_exists (rhc, inst);
  if (!was_empty)
  {
    remove_inst_from_nonempty_list (rhc, inst);
  }
  dds_tkmap_instance_unref (inst->tk);
  dds_free (inst);
}

uint32_t dds_rhc_lock_samples (struct rhc *rhc)
{
  uint32_t no;
  os_mutexLock (&rhc->lock);
  no = rhc->n_vsamples + rhc->n_invsamples;
  if (no == 0)
  {
    os_mutexUnlock (&rhc->lock);
  }
  return no;
}

void dds_rhc_free (struct rhc *rhc)
{
  assert (rhc_check_counts_locked (rhc, true));
  ut_hhEnum (rhc->instances, free_instance, rhc);
  assert (rhc->nonempty_instances == NULL);
  ut_hhFree (rhc->instances);
  lwregs_fini (&rhc->registrations);
  os_mutexDestroy (&rhc->lock);
  os_mutexDestroy (&rhc->conds_lock);
  dds_free (rhc);
}

void dds_rhc_fini (struct rhc * rhc)
{
  os_mutexLock (&rhc->lock);
  rhc->reader = NULL;
  os_mutexUnlock (&rhc->lock);

  /* Wait for all callbacks to complete */

  while (os_atomic_ld32 (&rhc->n_cbs) > 0)
  {
    dds_sleepfor (DDS_MSECS (1));
  }
}

static void init_trigger_info_nonmatch (struct trigger_info *info)
{
  info->qminst = ~0u;
  info->has_read = false;
  info->has_not_read = false;
  info->has_changed = false;
}

static void get_trigger_info (struct trigger_info *info, struct rhc_instance *inst, bool pre)
{
  info->qminst = qmask_of_inst (inst);
  info->has_read = INST_HAS_READ (inst);
  info->has_not_read = INST_HAS_UNREAD (inst);
  /* reset instance has_changed before adding/overwriting a sample */
  if (pre)
  {
    inst->has_changed = false;
  }
  info->has_changed = inst->has_changed;
}

static bool trigger_info_differs (const struct trigger_info *pre, const struct trigger_info *post)
{
  return pre->qminst != post->qminst || pre->has_read != post->has_read || pre->has_not_read != post->has_not_read ||
         pre->has_changed != post->has_changed;
}

static bool add_sample
(
  struct rhc * rhc,
  struct rhc_instance * inst,
  const struct nn_rsample_info * sampleinfo,
  const struct serdata * sample,
  status_cb_data_t * cb_data
)
{
  struct rhc_sample *s;
  assert (sample->v.bswap == sampleinfo->bswap);

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

    inst_clear_invsample_if_exists (rhc, inst);
    assert (inst->latest != NULL);
    s = inst->latest->next;
    ddsi_serdata_unref (s->sample);
    if (s->isread)
    {
      inst->nvread--;
      rhc->n_vread--;
    }

    /* set a flag to indicate instance has changed to notify data_available since the sample is overwritten */
    inst->has_changed = true;
  }
  else
  {
    /* Check if resource max_samples QoS exceeded */

    if (rhc->reader && rhc->max_samples != DDS_LENGTH_UNLIMITED && rhc->n_vsamples >= (uint32_t) rhc->max_samples)
    {
      cb_data->status = DDS_SAMPLE_REJECTED_STATUS;
      cb_data->extra = DDS_REJECTED_BY_SAMPLES_LIMIT;
      cb_data->handle = inst->iid;
      cb_data->add = true;
      return false;
    }

    /* Check if resource max_samples_per_instance QoS exceeded */

    if (rhc->reader && rhc->max_samples_per_instance != DDS_LENGTH_UNLIMITED && inst->nvsamples >= (uint32_t) rhc->max_samples_per_instance)
    {
      cb_data->status = DDS_SAMPLE_REJECTED_STATUS;
      cb_data->extra = DDS_REJECTED_BY_SAMPLES_PER_INSTANCE_LIMIT;
      cb_data->handle = inst->iid;
      cb_data->add = true;
      return false;
    }

    /* add new latest sample */

    s = alloc_sample (inst);
    inst_clear_invsample_if_exists (rhc, inst);
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

  s->sample = ddsi_serdata_ref ((serdata_t) sample); /* drops const (tho refcount does change) */
  s->wr_iid = sampleinfo->pwr_info.iid;
  s->rtstamp = sampleinfo->reception_timestamp;
  s->isread = false;
  s->disposed_gen = inst->disposed_gen;
  s->no_writers_gen = inst->no_writers_gen;
  inst->latest = s;

  return true;
}

static bool content_filter_accepts (const struct sertopic * topic, const struct serdata *sample)
{
  bool ret = true;

  if (topic->filter_fn)
  {
    deserialize_into ((char*) topic->filter_sample, sample);
    ret = (topic->filter_fn) (topic->filter_sample, topic->filter_ctx);
  }
  return ret;
}

static int inst_accepts_sample_by_writer_guid (const struct rhc_instance *inst, const struct nn_rsample_info *sampleinfo)
{
  return inst->wr_iid == sampleinfo->pwr_info.iid ||
    memcmp (&sampleinfo->pwr_info.guid, &inst->wr_guid, sizeof (inst->wr_guid)) < 0;
}

static int inst_accepts_sample
(
  const struct rhc *rhc, const struct rhc_instance *inst,
  const struct nn_rsample_info *sampleinfo,
  const struct serdata *sample, const bool has_data
)
{
  if (rhc->by_source_ordering)
  {
    if (sample->v.msginfo.timestamp.v > inst->tstamp.v)
    {
      /* ok */
    }
    else if (sample->v.msginfo.timestamp.v < inst->tstamp.v)
    {
      return 0;
    }
    else if (inst_accepts_sample_by_writer_guid (inst, sampleinfo))
    {
      /* ok */
    }
    else
    {
      return 0;
    }
  }
  if (rhc->exclusive_ownership && inst->wr_iid != sampleinfo->pwr_info.iid)
  {
    uint32_t strength = sampleinfo->pwr_info.ownership_strength;
    if (strength > inst->strength) {
      /* ok */
    } else if (strength < inst->strength) {
      return 0;
    } else if (inst_accepts_sample_by_writer_guid (inst, sampleinfo)) {
      /* ok */
    } else {
      return 0;
    }
  }
  if (has_data && !content_filter_accepts (rhc->topic, sample))
  {
    return 0;
  }
  return 1;
}

static void update_inst
(
  const struct rhc *rhc, struct rhc_instance *inst,
  const struct proxy_writer_info * __restrict pwr_info, nn_wctime_t tstamp)
{
  if (inst->wr_iid != pwr_info->iid)
  {
    inst->wr_guid = pwr_info->guid;
  }
  inst->tstamp = tstamp;
  inst->wr_iid = (inst->wrcount == 0) ? 0 : pwr_info->iid;
  inst->strength = pwr_info->ownership_strength;
}

static void drop_instance_noupdate_no_writers (struct rhc *rhc, struct rhc_instance *inst)
{
  int ret;
  assert (INST_IS_EMPTY (inst));

  rhc->n_instances--;

  ret = ut_hhRemove (rhc->instances, inst);
  assert (ret);
  (void) ret;

  free_instance (inst, rhc);
}

static void dds_rhc_register (struct rhc *rhc, struct rhc_instance *inst, uint64_t wr_iid, bool iid_update)
{
  TRACE ((" register:"));

  /* Is an implicitly registering dispose semantically equivalent to
     register ; dispose?  If so, both no_writers_gen and disposed_gen
     need to be incremented if the old instance state was DISPOSED,
     else just disposed_gen.  (Shudder.)  Interpreting it as
     equivalent.

     Is a dispose a sample?  I don't think so (though a write dispose
     is).  Is a pure register a sample?  Don't think so either. */
  if (inst->wr_iid == wr_iid)
  {
    /* Same writer as last time => we know it is registered already.
       This is the fast path -- we don't have to check anything
       else. */
    TRACE (("cached"));
    assert (inst->wrcount > 0);
    return;
  }

  if (inst->wrcount == 0)
  {
    /* Currently no writers at all */
    assert (inst->wr_iid == 0);

    /* to avoid wr_iid update when register is called for sample rejected */
    if (iid_update)
    {
      inst->wr_iid = wr_iid;
    }
    inst->wrcount++;
    inst->no_writers_gen++;
    TRACE (("new1"));

    if (!INST_IS_EMPTY (inst) && !inst->isdisposed)
      rhc->n_not_alive_no_writers--;
  }
  else if (inst->wr_iid == 0 && inst->wrcount == 1)
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
      TRACE (("new2iidnull"));
    }
    else
    {
      int x = lwregs_delete (&rhc->registrations, inst->iid, wr_iid);
      assert (x);
      (void) x;
      TRACE (("restore"));
    }
    /* to avoid wr_iid update when register is called for sample rejected */
    if (iid_update)
    {
      inst->wr_iid = wr_iid;
    }
  }
  else
  {
    /* As above -- if using concurrent hopscotch hashing, if the
       writer is already known, lwregs_add is lock-free */
    if (inst->wrcount == 1)
    {
      /* 2nd writer => properly register the one we knew about */
      TRACE (("rescue1"));
      int x;
      x = lwregs_add (&rhc->registrations, inst->iid, inst->wr_iid);
      assert (x);
      (void) x;
    }
    if (lwregs_add (&rhc->registrations, inst->iid, wr_iid))
    {
      /* as soon as we reach at least two writers, we have to check
         the result of lwregs_add to know whether this sample
         registers a previously unknown writer or not */
      TRACE (("new3"));
      inst->wrcount++;
    }
    else
    {
      TRACE (("known"));
    }
    assert (inst->wrcount >= 2);
    /* the most recent writer gets the fast path */
    /* to avoid wr_iid update when register is called for sample rejected */
    if (iid_update)
    {
      inst->wr_iid = wr_iid;
    }
  }
}

static void account_for_empty_to_nonempty_transition (struct rhc *rhc, struct rhc_instance *inst)
{
  assert (INST_NSAMPLES (inst) == 1);
  add_inst_to_nonempty_list (rhc, inst);
  if (inst->isnew)
  {
    rhc->n_new++;
  }
  if (inst->isdisposed)
    rhc->n_not_alive_disposed++;
  else if (inst->wrcount == 0)
    rhc->n_not_alive_no_writers++;
}

static int rhc_unregister_isreg_w_sideeffects (struct rhc *rhc, const struct rhc_instance *inst, uint64_t wr_iid)
{
  /* Returns 1 if last registration just disappeared */
  if (inst->wrcount == 0)
  {
    TRACE (("unknown(#0)"));
    return 0;
  }
  else if (inst->wrcount == 1 && inst->wr_iid != 0)
  {
    if (wr_iid != inst->wr_iid)
    {
      TRACE (("unknown(cache)"));
      return 0;
    }
    else
    {
      TRACE (("last(cache)"));
      return 1;
    }
  }
  else if (!lwregs_delete (&rhc->registrations, inst->iid, wr_iid))
  {
    TRACE (("unknown(regs)"));
    return 0;
  }
  else
  {
    TRACE (("delreg"));
    /* If we transition from 2 to 1 writer, and we are deleting a
       writer other than the one cached in the instance, that means
       afterward there will be 1 writer, it will be cached, and its
       registration record must go (invariant that with wrcount = 1
       and wr_iid != 0 the wr_iid is not in "registrations") */
    if (inst->wrcount == 2 && inst->wr_iid != wr_iid)
    {
      TRACE ((",delreg(remain)"));
      lwregs_delete (&rhc->registrations, inst->iid, inst->wr_iid);
    }
    return 1;
  }
}

static int rhc_unregister_updateinst
(
  struct rhc *rhc, struct rhc_instance *inst,
  const struct proxy_writer_info * __restrict pwr_info, nn_wctime_t tstamp)
{
  assert (inst->wrcount > 0);

  if (pwr_info->iid == inst->wr_iid)
  {
    /* Next register will have to do real work before we have a cached
       wr_iid again */
    inst->wr_iid = 0;

    /* Reset the ownership strength to allow samples to be read from other
     writer(s) */
    inst->strength = 0;
    TRACE ((",clearcache"));
  }

  if (--inst->wrcount > 0)
    return 0;
  else if (!INST_IS_EMPTY (inst))
  {
    /* Instance still has content - do not drop until application
       takes the last sample.  Set the invalid sample if the latest
       sample has been read already, so that the application can
       read the change to not-alive.  (If the latest sample is still
       unread, we don't bother, even though it means the application
       won't see the timestamp for the unregister event. It shouldn't
       care.) */
    if (inst->latest == NULL /*|| inst->latest->isread*/)
    {
      inst_set_invsample (rhc, inst);
      update_inst (rhc, inst, pwr_info, tstamp);
    }
    if (!inst->isdisposed)
    {
      rhc->n_not_alive_no_writers++;
    }
    return 0;
  }
  else if (inst->isdisposed)
  {
    /* No content left, no registrations left, so drop */
    TRACE ((",#0,empty,disposed,drop"));
    drop_instance_noupdate_no_writers (rhc, inst);
    return 1;
  }
  else
  {
    /* Add invalid samples for transition to no-writers */
    TRACE ((",#0,empty,nowriters"));
    assert (INST_IS_EMPTY (inst));
    inst_set_invsample (rhc, inst);
    update_inst (rhc, inst, pwr_info, tstamp);
    account_for_empty_to_nonempty_transition (rhc, inst);
    return 0;
  }
}

static void dds_rhc_unregister
(
  struct trigger_info *post, struct rhc *rhc, struct rhc_instance *inst,
  const struct proxy_writer_info * __restrict pwr_info, nn_wctime_t tstamp
)
{
  /* 'post' always gets set; instance may have been freed upon return. */
  TRACE ((" unregister:"));
  if (!rhc_unregister_isreg_w_sideeffects (rhc, inst, pwr_info->iid))
  {
    /* other registrations remain */
    get_trigger_info (post, inst, false);
  }
  else if (rhc_unregister_updateinst (rhc, inst, pwr_info, tstamp))
  {
    /* instance dropped */
    init_trigger_info_nonmatch (post);
  }
  else
  {
    /* no writers remain, but instance not empty */
    get_trigger_info (post, inst, false);
  }
}

static struct rhc_instance * alloc_new_instance
(
  const struct rhc *rhc,
  const struct nn_rsample_info *sampleinfo,
  struct serdata *serdata,
  struct tkmap_instance *tk
)
{
  struct rhc_instance *inst;

  dds_tkmap_instance_ref (tk);
  inst = dds_alloc (sizeof (*inst));
  inst->iid = tk->m_iid;
  inst->tk = tk;
  inst->wrcount = (serdata->v.msginfo.statusinfo & NN_STATUSINFO_UNREGISTER) ? 0 : 1;
  inst->isdisposed = (serdata->v.msginfo.statusinfo & NN_STATUSINFO_DISPOSE);
  inst->isnew = true;
  inst->inv_exists = 0;
  inst->inv_isread = 0; /* don't care */
  inst->a_sample_free = true;
  update_inst (rhc, inst, &sampleinfo->pwr_info, serdata->v.msginfo.timestamp);
  return inst;
}

static rhc_store_result_t rhc_store_new_instance
(
  struct trigger_info * post,
  struct rhc *rhc,
  const struct nn_rsample_info *sampleinfo,
  struct serdata *sample,
  struct tkmap_instance *tk,
  const bool has_data,
  status_cb_data_t * cb_data
)
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

  if (has_data && !content_filter_accepts (rhc->topic, sample))
  {
    return RHC_FILTERED;
  }
  /* Check if resource max_instances QoS exceeded */

  if (rhc->reader && rhc->max_instances != DDS_LENGTH_UNLIMITED && rhc->n_instances >= (uint32_t) rhc->max_instances)
  {
    cb_data->status = DDS_SAMPLE_REJECTED_STATUS;
    cb_data->extra = DDS_REJECTED_BY_INSTANCES_LIMIT;
    cb_data->handle = tk->m_iid;
    cb_data->add = true;
    return RHC_REJECTED;
  }

  inst = alloc_new_instance (rhc, sampleinfo, sample, tk);
  if (has_data)
  {
    if (!add_sample (rhc, inst, sampleinfo, sample, cb_data))
    {
      free_instance (inst, rhc);
      return RHC_REJECTED;
    }
  }
  else
  {
    if (inst->isdisposed) {
      inst_set_invsample(rhc, inst);
    }
  }

  account_for_empty_to_nonempty_transition (rhc, inst);
  ret = ut_hhAdd (rhc->instances, inst);
  assert (ret);
  (void) ret;
  rhc->n_instances++;
  get_trigger_info (post, inst, false);

  return RHC_STORED;
}

/*
  dds_rhc_store: DDSI up call into read cache to store new sample. Returns whether sample
  delivered (true unless a reliable sample rejected).
*/

bool dds_rhc_store
(
  struct rhc * __restrict rhc, const struct nn_rsample_info * __restrict sampleinfo,
  struct serdata * __restrict sample, struct tkmap_instance * __restrict tk
)
{
  const uint64_t wr_iid = sampleinfo->pwr_info.iid;
  const unsigned statusinfo = sample->v.msginfo.statusinfo;
  const bool has_data = (sample->v.st->kind == STK_DATA);
  const int is_dispose = (statusinfo & NN_STATUSINFO_DISPOSE) != 0;
  struct rhc_instance dummy_instance;
  struct rhc_instance *inst;
  struct trigger_info pre, post;
  bool trigger_waitsets;
  rhc_store_result_t stored;
  status_cb_data_t cb_data;   /* Callback data for reader status callback */
  bool notify_data_available = true;
  bool delivered = true;

  TRACE (("rhc_store(%"PRIx64",%"PRIx64" si %x has_data %d:", tk->m_iid, wr_iid, statusinfo, has_data));
  if (!has_data && statusinfo == 0)
  {
    /* Write with nothing but a key -- I guess that would be a
       register, which we do implicitly. (Currently DDSI2 won't allow
       it through anyway.) */
    TRACE ((" ignore explicit register)\n"));
    return delivered;
  }

  dummy_instance.iid = tk->m_iid;
  stored = RHC_FILTERED;
  cb_data.status = 0;

  os_mutexLock (&rhc->lock);

  inst = ut_hhLookup (rhc->instances, &dummy_instance);
  if (inst == NULL)
  {
    /* New instance for this reader.  If no data content -- not (also)
       a write -- ignore it, I think we can get away with ignoring dispose or unregisters
       on unknown instances.
     */
    if (!has_data && !is_dispose)
    {
      TRACE ((" disp/unreg on unknown instance"));
      goto error_or_nochange;
    }
    else
    {
      TRACE ((" new instance"));
      stored = rhc_store_new_instance (&post, rhc, sampleinfo, sample, tk, has_data, &cb_data);
      if (stored != RHC_STORED)
      {
        goto error_or_nochange;
      }
      init_trigger_info_nonmatch (&pre);
    }
  }
  else if (!inst_accepts_sample (rhc, inst, sampleinfo, sample, has_data))
  {
    /* Rejected samples (and disposes) should still register the writer;
       unregister *must* be processed, or we have a memory leak. (We
       will raise a SAMPLE_REJECTED, and indicate that the system should
       kill itself.)  Not letting instances go to ALIVE or NEW based on
       a rejected sample - (no one knows, it seemed) */
    TRACE ((" instance rejects sample"));


    get_trigger_info (&pre, inst, true);
    if (has_data || is_dispose)
    {
      dds_rhc_register (rhc, inst, wr_iid, false);
    }
    if (statusinfo & NN_STATUSINFO_UNREGISTER)
    {
      dds_rhc_unregister (&post, rhc, inst, &sampleinfo->pwr_info, sample->v.msginfo.timestamp);
    }
    else
    {
      get_trigger_info (&post, inst, false);
    }
    /* notify sample lost */

    cb_data.status = DDS_SAMPLE_LOST_STATUS;
    cb_data.extra = 0;
    cb_data.handle = 0;
    cb_data.add = true;
    goto error_or_nochange;

    /* FIXME: deadline (and other) QoS? */
  }
  else
  {
    get_trigger_info (&pre, inst, true);

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
      const bool was_empty = INST_IS_EMPTY (inst);
      int inst_became_disposed = 0;

      /* Not just an unregister, so a write and/or a dispose (possibly
         combined with an unregister).  Write & dispose create a
         registration and we always do that, even if we have to delete
         it immediately afterward.  It seems unlikely to be worth the
         effort of optimising this, but it can be done.  On failure
         (i.e., out-of-memory), abort the operation and hope that the
         caller can still notify the application.  */

      dds_rhc_register (rhc, inst, wr_iid, true);

      /* Sample arriving for a NOT_ALIVE instance => view state NEW */
      if (has_data && not_alive)
      {
        TRACE ((" notalive->alive"));
        inst->isnew = true;
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
        TRACE ((" disposed->notdisposed"));
        inst->isdisposed = false;
        inst->disposed_gen++;
      }
      if (is_dispose)
      {
        inst->isdisposed = true;
        inst_became_disposed = !old_isdisposed;
        TRACE ((" dispose(%d)", inst_became_disposed));
      }

      /* Only need to add a sample to the history if the input actually
         is a sample. */
      if (has_data)
      {
        TRACE ((" add_sample"));
        if (! add_sample (rhc, inst, sampleinfo, sample, &cb_data))
        {
          TRACE (("(reject)"));
          stored = RHC_REJECTED;
          goto error_or_nochange;
        }
      }

      /* If instance became disposed, add an invalid sample if there are no samples left */
      if (inst_became_disposed && (inst->latest == NULL ))
        inst_set_invsample (rhc, inst);

      update_inst (rhc, inst, &sampleinfo->pwr_info, sample->v.msginfo.timestamp);

      /* Can only add samples => only need to give special treatment
         to instances that were empty before.  It is, however, not
         guaranteed that we end up with a non-empty instance: for
         example, if the instance was disposed & empty, nothing
         changes. */
      if (inst->latest || inst_became_disposed)
      {
        if (was_empty)
        {
          /* general function is slightly slower than a specialised
             one, but perhaps it is wiser to use the general one */
          account_for_empty_to_nonempty_transition (rhc, inst);
        }
        else
        {
          rhc->n_not_alive_disposed += inst->isdisposed - old_isdisposed;
          rhc->n_new += (inst->isnew ? 1 : 0) - (old_isnew ? 1 : 0);
        }
      }
      else
      {
        assert (INST_IS_EMPTY (inst) == was_empty);
      }
    }

    assert (rhc_check_counts_locked (rhc, false));

    if (statusinfo & NN_STATUSINFO_UNREGISTER)
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
      dds_rhc_unregister (&post, rhc, inst, &sampleinfo->pwr_info, sample->v.msginfo.timestamp);
    }
    else
    {
      get_trigger_info (&post, inst, false);
    }
  }

  TRACE ((")\n"));

  /* do not send data available notification when an instance is dropped */
  if ((post.qminst == ~0u) && (post.has_read == 0) && (post.has_not_read == 0) && (post.has_changed == false))
  {
    notify_data_available = false;
  }
  trigger_waitsets = trigger_info_differs (&pre, &post)
    && update_conditions_locked (rhc, &pre, &post, sample);

  assert (rhc_check_counts_locked (rhc, true));

  os_mutexUnlock (&rhc->lock);

  if (notify_data_available && (trigger_info_differs (&pre, &post)))
  {
    if (rhc->reader && (rhc->reader->m_entity.m_status_enable & DDS_DATA_AVAILABLE_STATUS))
    {
      os_atomic_inc32 (&rhc->n_cbs);
      dds_reader_status_cb (&rhc->reader->m_entity, &dds_rhc_data_avail_cb_data);
      os_atomic_dec32 (&rhc->n_cbs);
    }
  }

  if (rhc->reader && trigger_waitsets)
  {
      dds_entity_status_signal((dds_entity*)(rhc->reader));
  }

  return delivered;

error_or_nochange:

  if (rhc->reliable && (stored == RHC_REJECTED))
  {
    delivered = false;
  }

  os_mutexUnlock (&rhc->lock);
  TRACE ((")\n"));

  /* Make any reader status callback */

  if (cb_data.status && rhc->reader && rhc->reader->m_entity.m_status_enable)
  {
    os_atomic_inc32 (&rhc->n_cbs);
    dds_reader_status_cb (&rhc->reader->m_entity, &cb_data);
    os_atomic_dec32 (&rhc->n_cbs);
  }

  return delivered;
}

void dds_rhc_unregister_wr
(
  struct rhc * __restrict rhc,
  const struct proxy_writer_info * __restrict pwr_info
)
{
  /* Only to be called when writer with ID WR_IID has died.

     If we require that it will NEVER be resurrected, i.e., that next
     time a new WR_IID will be used for the same writer, then we have
     all the time in the world to scan the cache & clean up and that
     we don't have to keep it locked all the time (even if we do it
     that way now).  */
  bool trigger_waitsets = false;
  struct rhc_instance *inst;
  struct ut_hhIter iter;
  const uint64_t wr_iid = pwr_info->iid;
  const int auto_dispose = pwr_info->auto_dispose;

  os_mutexLock (&rhc->lock);
  TRACE (("rhc_unregister_wr_iid(%"PRIx64",%d:\n", wr_iid, auto_dispose));
  for (inst = ut_hhIterFirst (rhc->instances, &iter); inst; inst = ut_hhIterNext (&iter))
  {
    if (inst->wr_iid == wr_iid || lwregs_contains (&rhc->registrations, inst->iid, wr_iid))
    {
      struct trigger_info pre, post;
      get_trigger_info (&pre, inst, true);

      TRACE (("  %"PRIx64":", inst->iid));

      assert (inst->wrcount > 0);
      if (auto_dispose && !inst->isdisposed)
      {
        inst->isdisposed = true;

        /* Set invalid sample for disposing it (unregister may also set it for unregistering) */
        if (inst->latest)
        {
          assert (!inst->inv_exists);
          rhc->n_not_alive_disposed++;
        }
        else
        {
          const bool was_empty = INST_IS_EMPTY (inst);
          inst_set_invsample (rhc, inst);
          update_inst (rhc, inst, pwr_info, inst->tstamp);
          if (was_empty)
            account_for_empty_to_nonempty_transition (rhc, inst);
          else
            rhc->n_not_alive_disposed++;
        }
      }

      dds_rhc_unregister (&post, rhc, inst, pwr_info, inst->tstamp);

      TRACE (("\n"));

      if (trigger_info_differs (&pre, &post))
      {
        if (update_conditions_locked (rhc, &pre, &post, NULL))
        {
          trigger_waitsets = true;
        }
      }

      assert (rhc_check_counts_locked (rhc, true));
    }
  }
  TRACE ((")\n"));
  os_mutexUnlock (&rhc->lock);

  if (trigger_waitsets)
  {
      dds_entity_status_signal((dds_entity*)(rhc->reader));
  }
}

void dds_rhc_relinquish_ownership (struct rhc * __restrict rhc, const uint64_t wr_iid)
{
  struct rhc_instance *inst;
  struct ut_hhIter iter;
  os_mutexLock (&rhc->lock);
  TRACE (("rhc_relinquish_ownership(%"PRIx64":\n", wr_iid));
  for (inst = ut_hhIterFirst (rhc->instances, &iter); inst; inst = ut_hhIterNext (&iter))
  {
    if (inst->wr_iid == wr_iid)
    {
      inst->wr_iid = 0;
    }
  }
  TRACE ((")\n"));
  assert (rhc_check_counts_locked (rhc, true));
  os_mutexUnlock (&rhc->lock);
}

/* STATUSES:

   sample:   ANY, READ, NOT_READ
   view:     ANY, NEW, NOT_NEW
   instance: ANY, ALIVE, NOT_ALIVE, NOT_ALIVE_NO_WRITERS, NOT_ALIVE_DISPOSED
*/

static unsigned qmask_of_inst (const struct rhc_instance *inst)
{
  unsigned qm = inst->isnew ? DDS_NEW_VIEW_STATE : DDS_NOT_NEW_VIEW_STATE;

  if (inst->isdisposed)
    qm |= DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE;
  else if (inst->wrcount > 0)
    qm |= DDS_ALIVE_INSTANCE_STATE;
  else
    qm |= DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE;

  return qm;
}

static unsigned qmask_from_dcpsquery (unsigned sample_states, unsigned view_states, unsigned instance_states)
{
  unsigned qminv = 0;

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

static unsigned qmask_from_mask_n_cond(uint32_t mask, dds_readcond* cond)
{
    unsigned qminv;
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
  si->source_timestamp = sample->sample->v.msginfo.timestamp.v;
  si->reception_timestamp = sample->rtstamp.v;
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

  /* Not storing the underlying "sample" so the reception time is lost */
  si->reception_timestamp = 0;
}

static void patch_generations (dds_sample_info_t *si, uint32_t last_of_inst)
{
  if (last_of_inst > 0)
  {
    const unsigned ref =
      si[last_of_inst].disposed_generation_count + si[last_of_inst].no_writers_generation_count;
    uint32_t i;
    assert (si[last_of_inst].sample_rank == 0);
    assert (si[last_of_inst].generation_rank == 0);
    for (i = 0; i < last_of_inst; i++)
    {
      si[i].sample_rank = last_of_inst - i;
      si[i].generation_rank = ref - (si[i].disposed_generation_count + si[i].no_writers_generation_count);
    }
  }
}

static int dds_rhc_read_w_qminv
(
  struct rhc *rhc, bool lock, void ** values, dds_sample_info_t *info_seq,
  uint32_t max_samples, unsigned qminv, dds_instance_handle_t handle, dds_readcond *cond
)
{
  bool trigger_waitsets = false;
  uint32_t n = 0;
  const struct dds_topic_descriptor * desc = (const struct dds_topic_descriptor *) rhc->topic->type;

  if (lock)
  {
    os_mutexLock (&rhc->lock);
  }

  TRACE (("read_w_qminv(%p,%p,%p,%u,%x) - inst %u nonempty %u disp %u nowr %u new %u samples %u+%u read %u+%u\n",
    (void *) rhc, (void *) values, (void *) info_seq, max_samples, qminv,
    rhc->n_instances, rhc->n_nonempty_instances, rhc->n_not_alive_disposed,
    rhc->n_not_alive_no_writers, rhc->n_new, rhc->n_vsamples, rhc->n_invsamples,
    rhc->n_vread, rhc->n_invread));

  if (rhc->nonempty_instances)
  {
    struct rhc_instance * inst = rhc->nonempty_instances->next;
    struct rhc_instance * const end = inst;
    do
    {
      if (handle == DDS_HANDLE_NIL || inst->iid == handle)
      {
        if (!INST_IS_EMPTY (inst) && (qmask_of_inst (inst) & qminv) == 0)
        {
          /* samples present & instance, view state matches */
          struct trigger_info pre, post;
          const unsigned nread = INST_NREAD (inst);
          const uint32_t n_first = n;
          get_trigger_info (&pre, inst, true);

          if (inst->latest)
          {
            struct rhc_sample *sample = inst->latest->next, * const end1 = sample;
            do
            {
              if ((QMASK_OF_SAMPLE (sample) & qminv) == 0)
              {
                /* sample state matches too */
                set_sample_info (info_seq + n, inst, sample);
                deserialize_into ((char*) values[n], sample->sample);
                if (cond == NULL
                    || (dds_entity_kind(cond->m_entity.m_hdl) != DDS_KIND_COND_QUERY)
                    || (cond->m_query.m_filter != NULL && cond->m_query.m_filter(values[n])))
                {
                  if (!sample->isread)
                  {
                    TRACE (("s"));
                    sample->isread = true;
                    inst->nvread++;
                    rhc->n_vread++;
                  }

                  if (++n == max_samples)
                  {
                    break;
                  }
                }
                else
                {
                  /* The filter didn't match, so free the deserialised copy. */
                  dds_sample_free(values[n], desc, DDS_FREE_CONTENTS);
                }
              }
              sample = sample->next;
            }
            while (sample != end1);
          }

          if (inst->inv_exists && n < max_samples && (QMASK_OF_INVSAMPLE (inst) & qminv) == 0)
          {
            set_sample_info_invsample (info_seq + n, inst);
            deserialize_into ((char*) values[n], inst->tk->m_sample);
            if (!inst->inv_isread)
            {
              inst->inv_isread = 1;
              rhc->n_invread++;
            }
            ++n;
          }

          if (n > n_first && inst->isnew)
          {
            inst->isnew = false;
            rhc->n_new--;
          }
          if (nread != INST_NREAD (inst))
          {
            get_trigger_info (&post, inst, false);
            if (update_conditions_locked (rhc, &pre, &post, NULL))
            {
              trigger_waitsets = true;
            }
          }

          if (n > n_first) {
              patch_generations (info_seq + n_first, n - n_first - 1);
          }
        }
        if (inst->iid == handle)
        {
          break;
        }
      }
      inst = inst->next;
    }
    while (inst != end && n < max_samples);
  }
  TRACE (("read: returning %u\n", n));
  assert (rhc_check_counts_locked (rhc, true));
  os_mutexUnlock (&rhc->lock);

  if (trigger_waitsets)
  {
      dds_entity_status_signal((dds_entity*)(rhc->reader));
  }

  return n;
}

static int dds_rhc_take_w_qminv
(
  struct rhc *rhc, bool lock, void ** values, dds_sample_info_t *info_seq,
  uint32_t max_samples, unsigned qminv, dds_instance_handle_t handle, dds_readcond *cond
)
{
  bool trigger_waitsets = false;
  uint64_t iid;
  uint32_t n = 0;
  const struct dds_topic_descriptor * desc = (const struct dds_topic_descriptor *) rhc->topic->type;

  if (lock)
  {
    os_mutexLock (&rhc->lock);
  }

  TRACE (("take_w_qminv(%p,%p,%p,%u,%x) - inst %u nonempty %u disp %u nowr %u new %u samples %u+%u read %u+%u\n",
    (void*) rhc, (void*) values, (void*) info_seq, max_samples, qminv,
    rhc->n_instances, rhc->n_nonempty_instances, rhc->n_not_alive_disposed,
    rhc->n_not_alive_no_writers, rhc->n_new, rhc->n_vsamples,
    rhc->n_invsamples, rhc->n_vread, rhc->n_invread));

  if (rhc->nonempty_instances)
  {
    struct rhc_instance *inst = rhc->nonempty_instances->next;
    unsigned n_insts = rhc->n_nonempty_instances;
    while (n_insts-- > 0 && n < max_samples)
    {
      struct rhc_instance * const inst1 = inst->next;
      iid = inst->iid;
      if (handle == DDS_HANDLE_NIL || iid == handle)
      {
        if (!INST_IS_EMPTY (inst) && (qmask_of_inst (inst) & qminv) == 0)
        {
          struct trigger_info pre, post;
          unsigned nvsamples = inst->nvsamples;
          const uint32_t n_first = n;
          get_trigger_info (&pre, inst, true);

          if (inst->latest)
          {
            struct rhc_sample * psample = inst->latest;
            struct rhc_sample * sample = psample->next;
            while (nvsamples--)
            {
              struct rhc_sample * const sample1 = sample->next;

              if ((QMASK_OF_SAMPLE (sample) & qminv) != 0)
              {
                psample = sample;
              }
              else
              {
                set_sample_info (info_seq + n, inst, sample);
                deserialize_into ((char*) values[n], sample->sample);
                if (cond == NULL
                    || (dds_entity_kind(cond->m_entity.m_hdl) != DDS_KIND_COND_QUERY)
                    || ( cond->m_query.m_filter != NULL && cond->m_query.m_filter(values[n])))
                {
                  rhc->n_vsamples--;
                  if (sample->isread)
                  {
                    inst->nvread--;
                    rhc->n_vread--;
                  }

                  if (--inst->nvsamples > 0)
                  {
                    if (inst->latest == sample) {
                      inst->latest = psample;
                    }
                    psample->next = sample1;
                  }
                  else
                  {
                    inst->latest = NULL;
                  }

                  free_sample (inst, sample);

                  if (++n == max_samples)
                  {
                    break;
                  }
                }
                else
                {
                  /* The filter didn't match, so free the deserialised copy. */
                  dds_sample_free(values[n], desc, DDS_FREE_CONTENTS);
                }
              }
              sample = sample1;
            }
          }

          if (inst->inv_exists && n < max_samples && (QMASK_OF_INVSAMPLE (inst) & qminv) == 0)
          {
            set_sample_info_invsample (info_seq + n, inst);
            deserialize_into ((char*) values[n], inst->tk->m_sample);
            inst_clear_invsample (rhc, inst);
            ++n;
          }

          if (n > n_first && inst->isnew)
          {
            inst->isnew = false;
            rhc->n_new--;
          }

          if (n > n_first)
          {
            /* if nsamples = 0, it won't match anything, so no need to do
               anything here for drop_instance_noupdate_no_writers */
            get_trigger_info (&post, inst, false);
            if (update_conditions_locked (rhc, &pre, &post, NULL))
            {
              trigger_waitsets = true;
            }
          }

          if (INST_IS_EMPTY (inst))
          {
            remove_inst_from_nonempty_list (rhc, inst);

            if (inst->isdisposed)
            {
              rhc->n_not_alive_disposed--;
            }
            if (inst->wrcount == 0)
            {
              TRACE (("take: iid %"PRIx64" #0,empty,drop\n", iid));
              if (!inst->isdisposed)
              {
                /* disposed has priority over no writers (why not just 2 bits?) */
                rhc->n_not_alive_no_writers--;
              }
              drop_instance_noupdate_no_writers (rhc, inst);
            }
          }

          if (n > n_first) {
              patch_generations (info_seq + n_first, n - n_first - 1);
          }
        }
        if (iid == handle)
        {
          break;
        }
      }
      inst = inst1;
    }
  }
  TRACE (("take: returning %u\n", n));
  assert (rhc_check_counts_locked (rhc, true));
  os_mutexUnlock (&rhc->lock);

  if (trigger_waitsets)
  {
      dds_entity_status_signal((dds_entity*)(rhc->reader));
  }

  return n;
}

static int dds_rhc_takecdr_w_qminv
(
 struct rhc *rhc, bool lock, struct serdata ** values, dds_sample_info_t *info_seq,
 uint32_t max_samples, unsigned qminv, dds_instance_handle_t handle, dds_readcond *cond
 )
{
  bool trigger_waitsets = false;
  uint64_t iid;
  uint32_t n = 0;

  if (lock)
  {
    os_mutexLock (&rhc->lock);
  }

  TRACE (("take_w_qminv(%p,%p,%p,%u,%x) - inst %u nonempty %u disp %u nowr %u new %u samples %u+%u read %u+%u\n",
          (void*) rhc, (void*) values, (void*) info_seq, max_samples, qminv,
          rhc->n_instances, rhc->n_nonempty_instances, rhc->n_not_alive_disposed,
          rhc->n_not_alive_no_writers, rhc->n_new, rhc->n_vsamples,
          rhc->n_invsamples, rhc->n_vread, rhc->n_invread));

  if (rhc->nonempty_instances)
  {
    struct rhc_instance *inst = rhc->nonempty_instances->next;
    unsigned n_insts = rhc->n_nonempty_instances;
    while (n_insts-- > 0 && n < max_samples)
    {
      struct rhc_instance * const inst1 = inst->next;
      iid = inst->iid;
      if (handle == DDS_HANDLE_NIL || iid == handle)
      {
        if (!INST_IS_EMPTY (inst) && (qmask_of_inst (inst) & qminv) == 0)
        {
          struct trigger_info pre, post;
          unsigned nvsamples = inst->nvsamples;
          const uint32_t n_first = n;
          get_trigger_info (&pre, inst, true);

          if (inst->latest)
          {
            struct rhc_sample * psample = inst->latest;
            struct rhc_sample * sample = psample->next;
            while (nvsamples--)
            {
              struct rhc_sample * const sample1 = sample->next;

              if ((QMASK_OF_SAMPLE (sample) & qminv) != 0)
              {
                psample = sample;
              }
              else
              {
                set_sample_info (info_seq + n, inst, sample);
                values[n] = ddsi_serdata_ref(sample->sample);
                rhc->n_vsamples--;
                if (sample->isread)
                {
                  inst->nvread--;
                  rhc->n_vread--;
                }
                free_sample (inst, sample);

                if (--inst->nvsamples > 0)
                {
                  psample->next = sample1;
                }
                else
                {
                  inst->latest = NULL;
                }

                if (++n == max_samples)
                {
                  break;
                }
              }
              sample = sample1;
            }
          }

          if (inst->inv_exists && n < max_samples && (QMASK_OF_INVSAMPLE (inst) & qminv) == 0)
          {
            set_sample_info_invsample (info_seq + n, inst);
            values[n] = ddsi_serdata_ref(inst->tk->m_sample);
            inst_clear_invsample (rhc, inst);
            ++n;
          }

          if (n > n_first && inst->isnew)
          {
            inst->isnew = false;
            rhc->n_new--;
          }

          if (n > n_first)
          {
            /* if nsamples = 0, it won't match anything, so no need to do
             anything here for drop_instance_noupdate_no_writers */
            get_trigger_info (&post, inst, false);
            if (update_conditions_locked (rhc, &pre, &post, NULL))
            {
              trigger_waitsets = true;
            }
          }

          if (INST_IS_EMPTY (inst))
          {
            remove_inst_from_nonempty_list (rhc, inst);

            if (inst->isdisposed)
            {
              rhc->n_not_alive_disposed--;
            }
            if (inst->wrcount == 0)
            {
              TRACE (("take: iid %"PRIx64" #0,empty,drop\n", iid));
              if (!inst->isdisposed)
              {
                /* disposed has priority over no writers (why not just 2 bits?) */
                rhc->n_not_alive_no_writers--;
              }
              drop_instance_noupdate_no_writers (rhc, inst);
            }
          }

          if (n > n_first) {
              patch_generations (info_seq + n_first, n - n_first - 1);
          }
        }
        if (iid == handle)
        {
          break;
        }
      }
      inst = inst1;
    }
  }
  TRACE (("take: returning %u\n", n));
  assert (rhc_check_counts_locked (rhc, true));
  os_mutexUnlock (&rhc->lock);

  if (trigger_waitsets)
  {
      dds_entity_status_signal((dds_entity*)(rhc->reader));
  }

  return n;
}

/*************************
 ******   WAITSET   ******
 *************************/

static uint32_t rhc_get_cond_trigger (struct rhc_instance * const inst, const dds_readcond * const c)
{
  bool m = ((qmask_of_inst (inst) & c->m_qminv) == 0);
  switch (c->m_sample_states)
  {
    case DDS_SST_READ:
      m = m && INST_HAS_READ (inst);
      break;
    case DDS_SST_NOT_READ:
      m = m && INST_HAS_UNREAD (inst);
      break;
    case DDS_SST_READ | DDS_SST_NOT_READ:
      case 0:
      /* note: we get here only if inst not empty, so this is a no-op */
      m = m && !INST_IS_EMPTY (inst);
      break;
    default:
      NN_FATAL ("update_readconditions: sample_states invalid: %x\n", c->m_sample_states);
  }
  return m ? 1 : 0;
}

void dds_rhc_add_readcondition (dds_readcond * cond)
{
  /* On the assumption that a readcondition will be attached to a
     waitset for nearly all of its life, we keep track of all
     readconditions on a reader in one set, without distinguishing
     between those attached to a waitset or not. */

  struct rhc * rhc = cond->m_rhc;
  struct ut_hhIter iter;
  struct rhc_instance * inst;

  cond->m_qminv = qmask_from_dcpsquery (cond->m_sample_states, cond->m_view_states, cond->m_instance_states);

  os_mutexLock (&rhc->lock);
  for (inst = ut_hhIterFirst (rhc->instances, &iter); inst; inst = ut_hhIterNext (&iter))
  {
    if (dds_entity_kind(cond->m_entity.m_hdl) == DDS_KIND_COND_READ)
    {
      ((dds_entity*)cond)->m_trigger += rhc_get_cond_trigger (inst, cond);
      if (((dds_entity*)cond)->m_trigger) {
        dds_entity_status_signal((dds_entity*)cond);
      }
    }
  }
  os_mutexLock (&rhc->conds_lock);
  cond->m_rhc_next = rhc->conds;
  rhc->nconds++;
  rhc->conds = cond;

  TRACE (("add_readcondition(%p, %x, %x, %x) => %p qminv %x ; rhc %u conds\n",
    (void *) rhc, cond->m_sample_states, cond->m_view_states,
    cond->m_instance_states, cond, cond->m_qminv, rhc->nconds));

  os_mutexUnlock (&rhc->conds_lock);
  os_mutexUnlock (&rhc->lock);
}

void dds_rhc_remove_readcondition (dds_readcond * cond)
{
  struct rhc * rhc = cond->m_rhc;
  dds_readcond * iter;
  dds_readcond * prev = NULL;

  os_mutexLock (&rhc->lock);
  os_mutexLock (&rhc->conds_lock);
  iter = rhc->conds;
  while (iter)
  {
    if (iter == cond)
    {
      if (prev)
      {
        prev->m_rhc_next = iter->m_rhc_next;
      }
      else
      {
        rhc->conds = iter->m_rhc_next;
      }
      rhc->nconds--;
      break;
    }
    prev = iter;
    iter = iter->m_rhc_next;
  }
  os_mutexUnlock (&rhc->conds_lock);
  os_mutexUnlock (&rhc->lock);
}

static bool update_conditions_locked
(
  struct rhc *rhc, const struct trigger_info *pre,
  const struct trigger_info *post,
  const struct serdata *sample
)
{
  /* Pre: rhc->lock held; returns 1 if triggering required, else 0. */
  bool trigger = false;
  dds_readcond * iter;
  int m_pre;
  int m_post;
  bool deserialised = (rhc->topic->filter_fn != NULL);

  TRACE (("update_conditions_locked(%p) - inst %u nonempty %u disp %u nowr %u new %u samples %u read %u\n",
          (void *) rhc, rhc->n_instances, rhc->n_nonempty_instances, rhc->n_not_alive_disposed,
          rhc->n_not_alive_no_writers, rhc->n_new, rhc->n_vsamples, rhc->n_vread));

  assert (rhc->n_nonempty_instances >= rhc->n_not_alive_disposed + rhc->n_not_alive_no_writers);
  assert (rhc->n_nonempty_instances >= rhc->n_new);
  assert (rhc->n_vsamples >= rhc->n_vread);

  iter = rhc->conds;
  while (iter)
  {
    m_pre = ((pre->qminst & iter->m_qminv) == 0);
    m_post = ((post->qminst & iter->m_qminv) == 0);

    /* FIXME: use bitmask? */
    switch (iter->m_sample_states)
    {
      case DDS_SST_READ:
        m_pre = m_pre && pre->has_read;
        m_post = m_post && post->has_read;
        break;
      case DDS_SST_NOT_READ:
        m_pre = m_pre && pre->has_not_read;
        m_post = m_post && post->has_not_read;
        break;
      case DDS_SST_READ | DDS_SST_NOT_READ:
      case 0:
        m_pre = m_pre && (pre->has_read + pre->has_not_read);
        m_post = m_post && (post->has_read + post->has_not_read);
        break;
      default:
        NN_FATAL ("update_readconditions: sample_states invalid: %x\n", iter->m_sample_states);
    }

    TRACE (("  cond %p: ", (void *) iter));
    if (m_pre == m_post)
    {
      TRACE (("no change"));
    }
    else if (m_pre < m_post)
    {
      if (sample && !deserialised && (dds_entity_kind(iter->m_entity.m_hdl) == DDS_KIND_COND_QUERY))
      {
        deserialize_into ((char*)rhc->topic->filter_sample, sample);
        deserialised = true;
      }
      if
      (
        (sample == NULL)
        || (dds_entity_kind(iter->m_entity.m_hdl) != DDS_KIND_COND_QUERY)
        || (iter->m_query.m_filter != NULL && iter->m_query.m_filter (rhc->topic->filter_sample))
      )
      {
        TRACE (("now matches"));
        if (iter->m_entity.m_trigger++ == 0)
        {
          TRACE ((" (cond now triggers)"));
          trigger = true;
        }
      }
    }
    else
    {
      TRACE (("no longer matches"));
      if (--iter->m_entity.m_trigger == 0)
      {
        TRACE ((" (cond no longer triggers)"));
      }
    }
    if (iter->m_entity.m_trigger) {
        dds_entity_status_signal(&(iter->m_entity));
    }

    TRACE (("\n"));
    iter = iter->m_rhc_next;
  }

  return trigger;
}


/*************************
 ******  READ/TAKE  ******
 *************************/

int
dds_rhc_read(
        struct rhc *rhc,
        bool lock,
        void ** values,
        dds_sample_info_t *info_seq,
        uint32_t max_samples,
        uint32_t mask,
        dds_instance_handle_t handle,
        dds_readcond *cond)
{
    unsigned qminv = qmask_from_mask_n_cond(mask, cond);
    return dds_rhc_read_w_qminv(rhc, lock, values, info_seq, max_samples, qminv, handle, cond);
}

int
dds_rhc_take(
        struct rhc *rhc,
        bool lock,
        void ** values,
        dds_sample_info_t *info_seq,
        uint32_t max_samples,
        uint32_t mask,
        dds_instance_handle_t handle,
        dds_readcond *cond)
{
    unsigned qminv = qmask_from_mask_n_cond(mask, cond);
    return dds_rhc_take_w_qminv(rhc, lock, values, info_seq, max_samples, qminv, handle, cond);
}

int dds_rhc_takecdr
(
 struct rhc *rhc, bool lock, struct serdata ** values, dds_sample_info_t *info_seq, uint32_t max_samples,
 unsigned sample_states, unsigned view_states, unsigned instance_states, dds_instance_handle_t handle)
{
  unsigned qminv = qmask_from_dcpsquery (sample_states, view_states, instance_states);
  return dds_rhc_takecdr_w_qminv (rhc, lock, values, info_seq, max_samples, qminv, handle, NULL);
}

/*************************
 ******    CHECK    ******
 *************************/

#ifndef NDEBUG
#define CHECK_MAX_CONDS 64
static int rhc_check_counts_locked (struct rhc *rhc, bool check_conds)
{
  unsigned n_instances = 0, n_nonempty_instances = 0;
  unsigned n_not_alive_disposed = 0, n_not_alive_no_writers = 0, n_new = 0;
  unsigned n_vsamples = 0, n_vread = 0;
  unsigned n_invsamples = 0, n_invread = 0;
  unsigned cond_match_count[CHECK_MAX_CONDS];
  struct rhc_instance *inst;
  struct ut_hhIter iter;
  uint32_t i;

  for (i = 0; i < CHECK_MAX_CONDS; i++)
    cond_match_count[i] = 0;

  for (inst = ut_hhIterFirst (rhc->instances, &iter); inst; inst = ut_hhIterNext (&iter))
  {
    n_instances++;
    if (!INST_IS_EMPTY (inst))
    {
      /* samples present (or an invalid sample is) */
      unsigned n_vsamples_in_instance = 0, n_read_vsamples_in_instance = 0;
      bool a_sample_free = true;

      n_nonempty_instances++;
      if (inst->isdisposed)
      {
        n_not_alive_disposed++;
      }
      else if (inst->wrcount == 0)
        n_not_alive_no_writers++;
      if (inst->isnew)
      {
        n_new++;
      }

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

      {
        dds_readcond * rciter = rhc->conds;
        for (i = 0; i < (rhc->nconds < CHECK_MAX_CONDS ? rhc->nconds : CHECK_MAX_CONDS); i++)
        {
          if (dds_entity_kind(rciter->m_entity.m_hdl) == DDS_KIND_COND_READ)
          {
            cond_match_count[i] += rhc_get_cond_trigger (inst, rciter);
          }
          rciter = rciter->m_rhc_next;
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
    dds_readcond * rciter = rhc->conds;
    for (i = 0; i < (rhc->nconds < CHECK_MAX_CONDS ? rhc->nconds : CHECK_MAX_CONDS); i++)
    {
      if (dds_entity_kind(rciter->m_entity.m_hdl) == DDS_KIND_COND_READ)
      {
        assert (cond_match_count[i] == rciter->m_entity.m_trigger);
      }
      rciter = rciter->m_rhc_next;
    }
  }

  if (rhc->n_nonempty_instances == 0)
  {
    assert (rhc->nonempty_instances == NULL);
  }
  else
  {
    struct rhc_instance *prev, *end;
    assert (rhc->nonempty_instances != NULL);
    prev = rhc->nonempty_instances->prev;
    end = rhc->nonempty_instances;
    inst = rhc->nonempty_instances;
    n_nonempty_instances = 0;
    do {
      assert (!INST_IS_EMPTY (inst));
      assert (prev->next == inst);
      assert (inst->prev == prev);
      prev = inst;
      inst = inst->next;
      n_nonempty_instances++;
    } while (inst != end);
    assert (rhc->n_nonempty_instances == n_nonempty_instances);
  }

  return 1;
}
#undef CHECK_MAX_CONDS
#endif
