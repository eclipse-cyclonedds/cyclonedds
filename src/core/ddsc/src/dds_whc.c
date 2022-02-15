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
#include <stddef.h>
#include <string.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/features.h"
#ifdef DDS_HAS_LIFESPAN
#include "dds/ddsi/ddsi_lifespan.h"
#endif
#ifdef DDS_HAS_DEADLINE_MISSED
#include "dds/ddsi/ddsi_deadline.h"
#endif
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/q_freelist.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_entity.h"
#include "dds__whc.h"
#include "dds__entity.h"
#include "dds__writer.h"

#define USE_EHH 0


struct whc_node {
  struct whc_node *next_seq; /* next in this interval */
  struct whc_node *prev_seq; /* prev in this interval */
  struct whc_idxnode *idxnode; /* NULL if not in index */
  uint32_t idxnode_pos; /* index in idxnode.hist */
  seqno_t seq;
  uint64_t total_bytes; /* cumulative number of bytes up to and including this node */
  size_t size;
  struct ddsi_plist *plist; /* 0 if nothing special */
  unsigned unacked: 1; /* counted in whc::unacked_bytes iff 1 */
  unsigned borrowed: 1; /* at most one can borrow it at any time */
  ddsrt_mtime_t last_rexmit_ts;
  uint32_t rexmit_count;
#ifdef DDS_HAS_LIFESPAN
  struct lifespan_fhnode lifespan; /* fibheap node for lifespan */
#endif
  struct ddsi_serdata *serdata;
};

struct whc_intvnode {
  ddsrt_avl_node_t avlnode;
  seqno_t min;
  seqno_t maxp1;
  struct whc_node *first; /* linked list of seqs with contiguous sequence numbers [min,maxp1) */
  struct whc_node *last; /* valid iff first != NULL */
};

struct whc_idxnode {
  uint64_t iid;
  seqno_t prune_seq;
  struct ddsi_tkmap_instance *tk;
  uint32_t headidx;
#ifdef DDS_HAS_DEADLINE_MISSED
  struct deadline_elem deadline; /* list element for deadline missed */
#endif
  struct whc_node *hist[];
};

#if USE_EHH
struct whc_seq_entry {
  seqno_t seq;
  struct whc_node *whcn;
};
#endif

struct whc_writer_info {
  dds_writer * writer; /* can be NULL, eg in case of whc for built-in writers */
  unsigned is_transient_local: 1;
  unsigned has_deadline: 1;
  uint32_t hdepth; /* 0 = unlimited */
  uint32_t tldepth; /* 0 = disabled/unlimited (no need to maintain an index if KEEP_ALL <=> is_transient_local + tldepth=0) */
  uint32_t idxdepth; /* = max (hdepth, tldepth) */
};

struct whc_impl {
  struct whc common;
  ddsrt_mutex_t lock;
  uint32_t seq_size;
  size_t unacked_bytes;
  size_t sample_overhead;
  uint32_t fragment_size;
  uint64_t total_bytes; /* total number of bytes pushed in */
  unsigned xchecks: 1;
  struct ddsi_domaingv *gv;
  struct ddsi_tkmap *tkmap;
  struct whc_writer_info wrinfo;
  seqno_t max_drop_seq; /* samples in whc with seq <= max_drop_seq => transient-local */
  struct whc_intvnode *open_intv; /* interval where next sample will go (usually) */
  struct whc_node *maxseq_node; /* NULL if empty; if not in open_intv, open_intv is empty */
#if USE_EHH
  struct ddsrt_ehh *seq_hash;
#else
  struct ddsrt_hh *seq_hash;
#endif
  struct ddsrt_hh *idx_hash;
  ddsrt_avl_tree_t seq;
#ifdef DDS_HAS_LIFESPAN
  struct lifespan_adm lifespan; /* Lifespan administration */
#endif
#ifdef DDS_HAS_DEADLINE_MISSED
  struct deadline_adm deadline; /* Deadline missed administration */
#endif
};

struct whc_sample_iter_impl {
  struct whc_sample_iter_base c;
  bool first;
};

/* check that our definition of whc_sample_iter fits in the type that callers allocate */
DDSRT_STATIC_ASSERT (sizeof (struct whc_sample_iter_impl) <= sizeof (struct whc_sample_iter));

/* Hash + interval tree adminitration of samples-by-sequence number
 * - by definition contains all samples in WHC (unchanged from older versions)
 * Circular array of samples per instance, inited to all 0
 * - length is max (durability_service.history_depth, history.depth), KEEP_ALL => as-if 0
 * - no instance index if above length 0
 * - each sample (i.e., whc_node): backpointer into index
 * - maintain index of latest sample, end of history then trivially follows from index arithmetic
 * Overwriting in insert drops them from index, depending on "aggressiveness" from by-seq
 * - special case for no readers (i.e. no ACKs) and history > transient-local history
 * - cleaning up after ACKs has additional pruning stage for same case
 */

static struct whc_node *whc_findseq (const struct whc_impl *whc, seqno_t seq);
static void insert_whcn_in_hash (struct whc_impl *whc, struct whc_node *whcn);
static void whc_delete_one (struct whc_impl *whc, struct whc_node *whcn);
static int compare_seq (const void *va, const void *vb);
static void free_deferred_free_list (struct whc_node *deferred_free_list);
static void get_state_locked (const struct whc_impl *whc, struct whc_state *st);

static uint32_t whc_default_remove_acked_messages_full (struct whc_impl *whc, seqno_t max_drop_seq, struct whc_node **deferred_free_list);
static uint32_t whc_default_remove_acked_messages (struct whc *whc, seqno_t max_drop_seq, struct whc_state *whcst, struct whc_node **deferred_free_list);
static void whc_default_free_deferred_free_list (struct whc *whc, struct whc_node *deferred_free_list);
static void whc_default_get_state (const struct whc *whc, struct whc_state *st);
static int whc_default_insert (struct whc *whc, seqno_t max_drop_seq, seqno_t seq, ddsrt_mtime_t exp, struct ddsi_plist *plist, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk);
static seqno_t whc_default_next_seq (const struct whc *whc, seqno_t seq);
static bool whc_default_borrow_sample (const struct whc *whc, seqno_t seq, struct whc_borrowed_sample *sample);
static bool whc_default_borrow_sample_key (const struct whc *whc, const struct ddsi_serdata *serdata_key, struct whc_borrowed_sample *sample);
static void whc_default_return_sample (struct whc *whc, struct whc_borrowed_sample *sample, bool update_retransmit_info);
static uint32_t whc_default_downgrade_to_volatile (struct whc *whc, struct whc_state *st);
static void whc_default_sample_iter_init (const struct whc *whc, struct whc_sample_iter *opaque_it);
static bool whc_default_sample_iter_borrow_next (struct whc_sample_iter *opaque_it, struct whc_borrowed_sample *sample);
static void whc_default_free (struct whc *whc);

static const ddsrt_avl_treedef_t whc_seq_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct whc_intvnode, avlnode), offsetof (struct whc_intvnode, min), compare_seq, 0);

static const struct whc_ops whc_ops = {
  .insert = whc_default_insert,
  .remove_acked_messages = whc_default_remove_acked_messages,
  .free_deferred_free_list = whc_default_free_deferred_free_list,
  .get_state = whc_default_get_state,
  .next_seq = whc_default_next_seq,
  .borrow_sample = whc_default_borrow_sample,
  .borrow_sample_key = whc_default_borrow_sample_key,
  .return_sample = whc_default_return_sample,
  .sample_iter_init = whc_default_sample_iter_init,
  .sample_iter_borrow_next = whc_default_sample_iter_borrow_next,
  .downgrade_to_volatile = whc_default_downgrade_to_volatile,
  .free = whc_default_free
};

#define TRACE(...) DDS_CLOG (DDS_LC_WHC, &whc->gv->logconfig, __VA_ARGS__)

/* Number of instantiated WHCs and a global freelist for WHC nodes that gets
 initialized lazily and cleaned up automatically when the last WHC is freed.
 Protected by dds_global.m_mutex.

 sizeof (whc_node) on 64-bit machines ~ 100 bytes, so this is ~1MB
 8k entries seems to be roughly the amount needed for minimum samples,
 maximum message size and a short round-trip time */
#define MAX_FREELIST_SIZE 8192
static uint32_t whc_count;
static struct nn_freelist whc_node_freelist;

#if USE_EHH
static uint32_t whc_seq_entry_hash (const void *vn)
{
  const struct whc_seq_entry *n = vn;
  /* we hash the lower 32 bits, on the assumption that with 4 billion
   samples in between there won't be significant correlation */
  const uint64_t c = UINT64_C (16292676669999574021);
  const uint32_t x = (uint32_t) n->seq;
  return (uint32_t) ((x * c) >> 32);
}

static int whc_seq_entry_eq (const void *va, const void *vb)
{
  const struct whc_seq_entry *a = va;
  const struct whc_seq_entry *b = vb;
  return a->seq == b->seq;
}
#else
static uint32_t whc_node_hash (const void *vn)
{
  const struct whc_node *n = vn;
  /* we hash the lower 32 bits, on the assumption that with 4 billion
   samples in between there won't be significant correlation */
  const uint64_t c = UINT64_C (16292676669999574021);
  const uint32_t x = (uint32_t) n->seq;
  return (uint32_t) ((x * c) >> 32);
}

static int whc_node_eq (const void *va, const void *vb)
{
  const struct whc_node *a = va;
  const struct whc_node *b = vb;
  return a->seq == b->seq;
}
#endif

static uint32_t whc_idxnode_hash_key (const void *vn)
{
  const struct whc_idxnode *n = vn;
  return (uint32_t)n->iid;
}

static int whc_idxnode_eq_key (const void *va, const void *vb)
{
  const struct whc_idxnode *a = va;
  const struct whc_idxnode *b = vb;
  return (a->iid == b->iid);
}

static int compare_seq (const void *va, const void *vb)
{
  const seqno_t *a = va;
  const seqno_t *b = vb;
  return (*a == *b) ? 0 : (*a < *b) ? -1 : 1;
}

static struct whc_node *whc_findmax_procedurally (const struct whc_impl *whc)
{
  if (whc->seq_size == 0)
    return NULL;
  else if (whc->open_intv->first)
  {
    /* last is only valid iff first != NULL */
    return whc->open_intv->last;
  }
  else
  {
    struct whc_intvnode *intv = ddsrt_avl_find_pred (&whc_seq_treedef, &whc->seq, whc->open_intv);
    assert (intv && intv->first);
    return intv->last;
  }
}

static void check_whc (const struct whc_impl *whc)
{
  /* there's much more we can check, but it gets expensive quite
   quickly: all nodes but open_intv non-empty, non-overlapping and
   non-contiguous; min & maxp1 of intervals correct; each interval
   contiguous; all samples in seq & in seqhash; tlidx \subseteq seq;
   seq-number ordered list correct; &c. */
  assert (whc->open_intv != NULL);
  assert (whc->open_intv == ddsrt_avl_find_max (&whc_seq_treedef, &whc->seq));
  assert (ddsrt_avl_find_succ (&whc_seq_treedef, &whc->seq, whc->open_intv) == NULL);
  if (whc->maxseq_node)
  {
    assert (whc->maxseq_node->next_seq == NULL);
  }
  if (whc->open_intv->first)
  {
    assert (whc->open_intv->last);
    assert (whc->maxseq_node == whc->open_intv->last);
    assert (whc->open_intv->min < whc->open_intv->maxp1);
    assert (whc->maxseq_node->seq + 1 == whc->open_intv->maxp1);
  }
  else
  {
    assert (whc->open_intv->min == whc->open_intv->maxp1);
  }
  assert (whc->maxseq_node == whc_findmax_procedurally (whc));

#if !defined (NDEBUG)
  if (whc->xchecks)
  {
    struct whc_intvnode *firstintv;
    struct whc_node *cur;
    seqno_t prevseq = 0;
    firstintv = ddsrt_avl_find_min (&whc_seq_treedef, &whc->seq);
    assert (firstintv);
    cur = firstintv->first;
    while (cur)
    {
      assert (cur->seq > prevseq);
      prevseq = cur->seq;
      assert (whc_findseq (whc, cur->seq) == cur);
      cur = cur->next_seq;
    }
  }
#endif
}

static void insert_whcn_in_hash (struct whc_impl *whc, struct whc_node *whcn)
{
  /* precondition: whcn is not in hash */
#if USE_EHH
  struct whc_seq_entry e = { .seq = whcn->seq, .whcn = whcn };
  if (!ddsrt_ehh_add (whc->seq_hash, &e))
    assert (0);
#else
  ddsrt_hh_add_absent (whc->seq_hash, whcn);
#endif
}

static void remove_whcn_from_hash (struct whc_impl *whc, struct whc_node *whcn)
{
  /* precondition: whcn is in hash */
#if USE_EHH
  struct whc_seq_entry e = { .seq = whcn->seq };
  if (!ddsrt_ehh_remove (whc->seq_hash, &e))
    assert (0);
#else
  ddsrt_hh_remove_present (whc->seq_hash, whcn);
#endif
}

static struct whc_node *whc_findseq (const struct whc_impl *whc, seqno_t seq)
{
#if USE_EHH
  struct whc_seq_entry e = { .seq = seq }, *r;
  if ((r = ddsrt_ehh_lookup (whc->seq_hash, &e)) != NULL)
    return r->whcn;
  else
    return NULL;
#else
  struct whc_node template;
  template.seq = seq;
  return ddsrt_hh_lookup (whc->seq_hash, &template);
#endif
}

static struct whc_node *whc_findkey (const struct whc_impl *whc, const struct ddsi_serdata *serdata_key)
{
  union {
    struct whc_idxnode idxn;
    char pad[sizeof (struct whc_idxnode) + sizeof (struct whc_node *)];
  } template;
  struct whc_idxnode *n;
  check_whc (whc);
  template.idxn.iid = ddsi_tkmap_lookup (whc->tkmap, serdata_key);
  n = ddsrt_hh_lookup (whc->idx_hash, &template.idxn);
  if (n == NULL)
    return NULL;
  else
  {
    assert (n->hist[n->headidx]);
    return n->hist[n->headidx];
  }
}

#ifdef DDS_HAS_LIFESPAN
static ddsrt_mtime_t whc_sample_expired_cb(void *hc, ddsrt_mtime_t tnow)
{
  struct whc_impl *whc = hc;
  void *sample;
  ddsrt_mtime_t tnext;
  ddsrt_mutex_lock (&whc->lock);
  while ((tnext = lifespan_next_expired_locked (&whc->lifespan, tnow, &sample)).v == 0)
    whc_delete_one (whc, sample);
  whc->maxseq_node = whc_findmax_procedurally (whc);
  ddsrt_mutex_unlock (&whc->lock);
  return tnext;
}
#endif

#ifdef DDS_HAS_DEADLINE_MISSED
static ddsrt_mtime_t whc_deadline_missed_cb(void *hc, ddsrt_mtime_t tnow)
{
  struct whc_impl *whc = hc;
  void *vidxnode;
  ddsrt_mtime_t tnext;
  ddsrt_mutex_lock (&whc->lock);
  while ((tnext = deadline_next_missed_locked (&whc->deadline, tnow, &vidxnode)).v == 0)
  {
    struct whc_idxnode *idxnode = vidxnode;
    deadline_reregister_instance_locked (&whc->deadline, &idxnode->deadline, tnow);

    status_cb_data_t cb_data;
    cb_data.raw_status_id = (int) DDS_OFFERED_DEADLINE_MISSED_STATUS_ID;
    cb_data.extra = 0;
    cb_data.handle = 0;
    cb_data.add = true;
    ddsrt_mutex_unlock (&whc->lock);
    dds_writer_status_cb (&whc->wrinfo.writer->m_entity, &cb_data);
    ddsrt_mutex_lock (&whc->lock);

    tnow = ddsrt_time_monotonic ();
  }
  ddsrt_mutex_unlock (&whc->lock);
  return tnext;
}
#endif

struct whc_writer_info *whc_make_wrinfo (struct dds_writer *wr, const dds_qos_t *qos)
{
  struct whc_writer_info *wrinfo = ddsrt_malloc (sizeof (*wrinfo));
  assert (qos->present & QP_HISTORY);
  assert (qos->present & QP_DEADLINE);
  assert (qos->present & QP_DURABILITY);
  assert (qos->present & QP_DURABILITY_SERVICE);
  wrinfo->writer = wr;
  wrinfo->is_transient_local = (qos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL);
  wrinfo->has_deadline = (qos->deadline.deadline != DDS_INFINITY);
  wrinfo->hdepth = (qos->history.kind == DDS_HISTORY_KEEP_ALL) ? 0 : (unsigned) qos->history.depth;
  if (!wrinfo->is_transient_local)
    wrinfo->tldepth = 0;
  else
    wrinfo->tldepth = (qos->durability_service.history.kind == DDS_HISTORY_KEEP_ALL) ? 0 : (unsigned) qos->durability_service.history.depth;
  wrinfo->idxdepth = wrinfo->hdepth > wrinfo->tldepth ? wrinfo->hdepth : wrinfo->tldepth;
  return wrinfo;
}

void whc_free_wrinfo (struct whc_writer_info *wrinfo)
{
  ddsrt_free (wrinfo);
}

struct whc *whc_new (struct ddsi_domaingv *gv, const struct whc_writer_info *wrinfo)
{
  size_t sample_overhead = 80; /* INFO_TS, DATA (estimate), inline QoS */
  struct whc_impl *whc;
  struct whc_intvnode *intv;

  assert ((wrinfo->hdepth == 0 || wrinfo->tldepth <= wrinfo->hdepth) || wrinfo->is_transient_local);

  whc = ddsrt_malloc (sizeof (*whc));
  whc->common.ops = &whc_ops;
  ddsrt_mutex_init (&whc->lock);
  whc->xchecks = (gv->config.enabled_xchecks & DDSI_XCHECK_WHC) != 0;
  whc->gv = gv;
  whc->tkmap = gv->m_tkmap;
  memcpy (&whc->wrinfo, wrinfo, sizeof (*wrinfo));
  whc->seq_size = 0;
  whc->max_drop_seq = 0;
  whc->unacked_bytes = 0;
  whc->total_bytes = 0;
  whc->sample_overhead = sample_overhead;
  whc->fragment_size = gv->config.fragment_size;
  whc->idx_hash = ddsrt_hh_new (1, whc_idxnode_hash_key, whc_idxnode_eq_key);
#if USE_EHH
  whc->seq_hash = ddsrt_ehh_new (sizeof (struct whc_seq_entry), 32, whc_seq_entry_hash, whc_seq_entry_eq);
#else
  whc->seq_hash = ddsrt_hh_new (1, whc_node_hash, whc_node_eq);
#endif

#ifdef DDS_HAS_LIFESPAN
  lifespan_init (gv, &whc->lifespan, offsetof(struct whc_impl, lifespan), offsetof(struct whc_node, lifespan), whc_sample_expired_cb);
#endif

#ifdef DDS_HAS_DEADLINE_MISSED
  whc->deadline.dur = (wrinfo->writer != NULL) ? wrinfo->writer->m_entity.m_qos->deadline.deadline : DDS_INFINITY;
  deadline_init (gv, &whc->deadline, offsetof(struct whc_impl, deadline), offsetof(struct whc_idxnode, deadline), whc_deadline_missed_cb);
#endif

  /* seq interval tree: always has an "open" node */
  ddsrt_avl_init (&whc_seq_treedef, &whc->seq);
  intv = ddsrt_malloc (sizeof (*intv));
  intv->min = intv->maxp1 = 1;
  intv->first = intv->last = NULL;
  ddsrt_avl_insert (&whc_seq_treedef, &whc->seq, intv);
  whc->open_intv = intv;
  whc->maxseq_node = NULL;

  ddsrt_mutex_lock (&dds_global.m_mutex);
  if (whc_count++ == 0)
    nn_freelist_init (&whc_node_freelist, MAX_FREELIST_SIZE, offsetof (struct whc_node, next_seq));
  ddsrt_mutex_unlock (&dds_global.m_mutex);

  check_whc (whc);
  return (struct whc *)whc;
}

static void free_whc_node_contents (struct whc_node *whcn)
{
  ddsi_serdata_unref (whcn->serdata);
  if (whcn->plist) {
    ddsi_plist_fini (whcn->plist);
    ddsrt_free (whcn->plist);
  }
}

void whc_default_free (struct whc *whc_generic)
{
  /* Freeing stuff without regards for maintaining data structures */
  struct whc_impl * const whc = (struct whc_impl *)whc_generic;
  check_whc (whc);

#ifdef DDS_HAS_LIFESPAN
  whc_sample_expired_cb (whc, DDSRT_MTIME_NEVER);
  lifespan_fini (&whc->lifespan);
#endif

#ifdef DDS_HAS_DEADLINE_MISSED
  deadline_stop (&whc->deadline);
  ddsrt_mutex_lock (&whc->lock);
  deadline_clear (&whc->deadline);
  ddsrt_mutex_unlock (&whc->lock);
  deadline_fini (&whc->deadline);
#endif

  struct ddsrt_hh_iter it;
  struct whc_idxnode *idxn;
  for (idxn = ddsrt_hh_iter_first (whc->idx_hash, &it); idxn != NULL; idxn = ddsrt_hh_iter_next (&it))
    ddsrt_free (idxn);
  ddsrt_hh_free (whc->idx_hash);

  {
    struct whc_node *whcn = whc->maxseq_node;
    while (whcn)
    {
      struct whc_node *tmp = whcn;
      /* The compiler doesn't realize that whcn->prev_seq is always initialized. */
      DDSRT_WARNING_MSVC_OFF (6001);
      whcn = whcn->prev_seq;
      DDSRT_WARNING_MSVC_ON (6001);
      free_whc_node_contents (tmp);
      ddsrt_free (tmp);
    }
  }

  ddsrt_avl_free (&whc_seq_treedef, &whc->seq, ddsrt_free);

  ddsrt_mutex_lock (&dds_global.m_mutex);
  if (--whc_count == 0)
    nn_freelist_fini (&whc_node_freelist, ddsrt_free);
  ddsrt_mutex_unlock (&dds_global.m_mutex);

#if USE_EHH
  ddsrt_ehh_free (whc->seq_hash);
#else
  ddsrt_hh_free (whc->seq_hash);
#endif
  ddsrt_mutex_destroy (&whc->lock);
  ddsrt_free (whc);
}

static void get_state_locked (const struct whc_impl *whc, struct whc_state *st)
{
  if (whc->seq_size == 0)
  {
    st->min_seq = st->max_seq = 0;
    st->unacked_bytes = 0;
  }
  else
  {
    const struct whc_intvnode *intv;
    intv = ddsrt_avl_find_min (&whc_seq_treedef, &whc->seq);
    /* not empty, open node may be anything but is (by definition)
     findmax, and whc is claimed to be non-empty, so min interval
     can't be empty */
    assert (intv->maxp1 > intv->min);
    st->min_seq = intv->min;
    st->max_seq = whc->maxseq_node->seq;
    st->unacked_bytes = whc->unacked_bytes;
  }
}

static void whc_default_get_state (const struct whc *whc_generic, struct whc_state *st)
{
  const struct whc_impl * const whc = (const struct whc_impl *)whc_generic;
  ddsrt_mutex_lock ((ddsrt_mutex_t *)&whc->lock);
  check_whc (whc);
  get_state_locked (whc, st);
  ddsrt_mutex_unlock ((ddsrt_mutex_t *)&whc->lock);
}

static struct whc_node *find_nextseq_intv (struct whc_intvnode **p_intv, const struct whc_impl *whc, seqno_t seq)
{
  struct whc_node *n;
  struct whc_intvnode *intv;
  if ((n = whc_findseq (whc, seq)) == NULL)
  {
    /* don't know seq => lookup interval with min > seq (intervals are
     contiguous, so if we don't know seq, an interval [X,Y) with X <
     SEQ < Y can't exist */
#ifndef NDEBUG
    {
      struct whc_intvnode *predintv = ddsrt_avl_lookup_pred_eq (&whc_seq_treedef, &whc->seq, &seq);
      assert (predintv == NULL || predintv->maxp1 <= seq);
    }
#endif
    if ((intv = ddsrt_avl_lookup_succ_eq (&whc_seq_treedef, &whc->seq, &seq)) == NULL) {
      assert (ddsrt_avl_lookup_pred_eq (&whc_seq_treedef, &whc->seq, &seq) == whc->open_intv);
      return NULL;
    } else if (intv->min < intv->maxp1) { /* only if not empty interval */
      assert (intv->min > seq);
      *p_intv = intv;
      return intv->first;
    } else { /* but note: only open_intv may be empty */
      assert (intv == whc->open_intv);
      return NULL;
    }
  }
  else if (n->next_seq == NULL)
  {
    assert (n == whc->maxseq_node);
    return NULL;
  }
  else
  {
    assert (whc->maxseq_node != NULL);
    assert (n->seq < whc->maxseq_node->seq);
    n = n->next_seq;
    *p_intv = ddsrt_avl_lookup_pred_eq (&whc_seq_treedef, &whc->seq, &n->seq);
    return n;
  }
}

static seqno_t whc_default_next_seq (const struct whc *whc_generic, seqno_t seq)
{
  const struct whc_impl * const whc = (const struct whc_impl *)whc_generic;
  struct whc_node *n;
  struct whc_intvnode *intv;
  seqno_t nseq;
  ddsrt_mutex_lock ((ddsrt_mutex_t *)&whc->lock);
  check_whc (whc);
  if ((n = find_nextseq_intv (&intv, whc, seq)) == NULL)
    nseq = MAX_SEQ_NUMBER;
  else
    nseq = n->seq;
  ddsrt_mutex_unlock ((ddsrt_mutex_t *)&whc->lock);
  return nseq;
}

static void delete_one_sample_from_idx (struct whc_node *whcn)
{
  struct whc_idxnode * const idxn = whcn->idxnode;
  assert (idxn != NULL);
  assert (idxn->hist[idxn->headidx] != NULL);
  assert (idxn->hist[whcn->idxnode_pos] == whcn);
  idxn->hist[whcn->idxnode_pos] = NULL;
  whcn->idxnode = NULL;
}

static void free_one_instance_from_idx (struct whc_impl *whc, seqno_t max_drop_seq, struct whc_idxnode *idxn)
{
  for (uint32_t i = 0; i < whc->wrinfo.idxdepth; i++)
  {
    if (idxn->hist[i])
    {
      struct whc_node *oldn = idxn->hist[i];
      oldn->idxnode = NULL;
      if (oldn->seq <= max_drop_seq)
      {
        TRACE ("  prune tl whcn %p\n", (void *)oldn);
        assert (oldn != whc->maxseq_node);
        whc_delete_one (whc, oldn);
      }
    }
  }
  ddsrt_free (idxn);
}

static void delete_one_instance_from_idx (struct whc_impl *whc, seqno_t max_drop_seq, struct whc_idxnode *idxn)
{
  ddsrt_hh_remove_present (whc->idx_hash, idxn);
#ifdef DDS_HAS_DEADLINE_MISSED
  deadline_unregister_instance_locked (&whc->deadline, &idxn->deadline);
#endif
  free_one_instance_from_idx (whc, max_drop_seq, idxn);
}

static int whcn_in_tlidx (const struct whc_impl *whc, const struct whc_idxnode *idxn, uint32_t pos)
{
  if (idxn == NULL)
    return 0;
  else
  {
    uint32_t d = (idxn->headidx + (pos > idxn->headidx ? whc->wrinfo.idxdepth : 0)) - pos;
    assert (d < whc->wrinfo.idxdepth);
    return d < whc->wrinfo.tldepth;
  }
}

static uint32_t whc_default_downgrade_to_volatile (struct whc *whc_generic, struct whc_state *st)
{
  struct whc_impl * const whc = (struct whc_impl *)whc_generic;
  seqno_t old_max_drop_seq;
  struct whc_node *deferred_free_list;
  uint32_t cnt;

  /* We only remove them from whc->tlidx: we don't remove them from
   whc->seq yet.  That'll happen eventually.  */
  ddsrt_mutex_lock (&whc->lock);
  check_whc (whc);

  if (whc->wrinfo.idxdepth == 0)
  {
    /* if not maintaining an index at all, this is nonsense */
    get_state_locked (whc, st);
    ddsrt_mutex_unlock (&whc->lock);
    return 0;
  }

  assert (!whc->wrinfo.is_transient_local);
  if (whc->wrinfo.tldepth > 0)
  {
    assert (whc->wrinfo.hdepth == 0 || whc->wrinfo.tldepth <= whc->wrinfo.hdepth);
    whc->wrinfo.tldepth = 0;
    if (whc->wrinfo.hdepth == 0)
    {
      struct ddsrt_hh_iter it;
      struct whc_idxnode *idxn;
      for (idxn = ddsrt_hh_iter_first (whc->idx_hash, &it); idxn != NULL; idxn = ddsrt_hh_iter_next (&it))
      {
#ifdef DDS_HAS_DEADLINE_MISSED
        deadline_unregister_instance_locked (&whc->deadline, &idxn->deadline);
#endif
        free_one_instance_from_idx (whc, 0, idxn);
      }
      ddsrt_hh_free (whc->idx_hash);
      whc->wrinfo.idxdepth = 0;
      whc->idx_hash = NULL;
    }
  }

  /* Immediately drop them from the WHC (used to delay it until the
   next ack); but need to make sure remove_acked_messages processes
   them all. */
  old_max_drop_seq = whc->max_drop_seq;
  whc->max_drop_seq = 0;
  cnt = whc_default_remove_acked_messages_full (whc, old_max_drop_seq, &deferred_free_list);
  whc_default_free_deferred_free_list (whc_generic, deferred_free_list);
  assert (whc->max_drop_seq == old_max_drop_seq);
  get_state_locked (whc, st);
  ddsrt_mutex_unlock (&whc->lock);
  return cnt;
}

static size_t whcn_size (const struct whc_impl *whc, const struct whc_node *whcn)
{
  size_t sz = ddsi_serdata_size (whcn->serdata);
  return sz + ((sz + whc->fragment_size - 1) / whc->fragment_size) * whc->sample_overhead;
}

static void whc_delete_one_intv (struct whc_impl *whc, struct whc_intvnode **p_intv, struct whc_node **p_whcn)
{
  /* Removes *p_whcn, possibly deleting or splitting *p_intv, as the
   case may be.  Does *NOT* update whc->seq_size.  *p_intv must be
   the interval containing *p_whcn (&& both must actually exist).

   Returns:
   - 0 if delete failed (only possible cause is memory exhaustion),
   in which case *p_intv & *p_whcn are undefined;
   - 1 if successful, in which case *p_intv & *p_whcn are set
   correctly for the next sample in sequence number order */
  struct whc_intvnode *intv = *p_intv;
  struct whc_node *whcn = *p_whcn;
  assert (whcn->seq >= intv->min && whcn->seq < intv->maxp1);
  *p_whcn = whcn->next_seq;

  /* If it is in the tlidx, take it out.  Transient-local data never gets here */
  if (whcn->idxnode)
    delete_one_sample_from_idx (whcn);
  if (whcn->unacked)
  {
    assert (whc->unacked_bytes >= whcn->size);
    whc->unacked_bytes -= whcn->size;
    whcn->unacked = 0;
  }

#ifdef DDS_HAS_LIFESPAN
  lifespan_unregister_sample_locked (&whc->lifespan, &whcn->lifespan);
#endif

  /* Take it out of seqhash; deleting it from the list ordered on
   sequence numbers is left to the caller (it has to be done unconditionally,
   but remove_acked_messages defers it until the end or a skipped node). */
  remove_whcn_from_hash (whc, whcn);

  /* We may have introduced a hole & have to split the interval
   node, or we may have nibbled of the first one, or even the
   last one. */
  if (whcn == intv->first)
  {
    if (whcn == intv->last && intv != whc->open_intv)
    {
      struct whc_intvnode *tmp = intv;
      *p_intv = ddsrt_avl_find_succ (&whc_seq_treedef, &whc->seq, intv);
      /* only sample in interval and not the open interval => delete interval */
      ddsrt_avl_delete (&whc_seq_treedef, &whc->seq, tmp);
      ddsrt_free (tmp);
    }
    else
    {
      intv->first = whcn->next_seq;
      intv->min++;
      assert (intv->first != NULL || intv == whc->open_intv);
      assert (intv->min < intv->maxp1 || intv == whc->open_intv);
      assert ((intv->first == NULL) == (intv->min == intv->maxp1));
    }
  }
  else if (whcn == intv->last)
  {
    /* well, at least it isn't the first one & so the interval is
     still non-empty and we don't have to drop the interval */
    assert (intv->min < whcn->seq);
    assert (whcn->prev_seq);
    assert (whcn->prev_seq->seq + 1 == whcn->seq);
    intv->last = whcn->prev_seq;
    intv->maxp1--;
    *p_intv = ddsrt_avl_find_succ (&whc_seq_treedef, &whc->seq, intv);
  }
  else
  {
    /* somewhere in the middle => split the interval (ideally,
     would split it lazily, but it really is a transient-local
     issue only, and so we can (for now) get away with splitting
     it greedily */
    struct whc_intvnode *new_intv;
    ddsrt_avl_ipath_t path;

    new_intv = ddsrt_malloc (sizeof (*new_intv));

    /* new interval starts at the next node */
    assert (whcn->next_seq);
    assert (whcn->seq + 1 == whcn->next_seq->seq);
    new_intv->first = whcn->next_seq;
    new_intv->last = intv->last;
    new_intv->min = whcn->seq + 1;
    new_intv->maxp1 = intv->maxp1;
    intv->last = whcn->prev_seq;
    intv->maxp1 = whcn->seq;
    assert (intv->min < intv->maxp1);
    assert (new_intv->min < new_intv->maxp1);

    /* insert new node & continue the loop with intv set to the
    new interval */
    if (ddsrt_avl_lookup_ipath (&whc_seq_treedef, &whc->seq, &new_intv->min, &path) != NULL)
      assert (0);
    ddsrt_avl_insert_ipath (&whc_seq_treedef, &whc->seq, new_intv, &path);

    if (intv == whc->open_intv)
      whc->open_intv = new_intv;
    *p_intv = new_intv;
  }
}

static void whc_delete_one (struct whc_impl *whc, struct whc_node *whcn)
{
  struct whc_intvnode *intv;
  struct whc_node *whcn_tmp = whcn;
  intv = ddsrt_avl_lookup_pred_eq (&whc_seq_treedef, &whc->seq, &whcn->seq);
  assert (intv != NULL);
  whc_delete_one_intv (whc, &intv, &whcn);
  if (whcn_tmp->prev_seq)
    whcn_tmp->prev_seq->next_seq = whcn_tmp->next_seq;
  if (whcn_tmp->next_seq)
    whcn_tmp->next_seq->prev_seq = whcn_tmp->prev_seq;
  whcn_tmp->next_seq = NULL;
  free_deferred_free_list (whcn_tmp);
  whc->seq_size--;
}

static void free_deferred_free_list (struct whc_node *deferred_free_list)
{
  if (deferred_free_list)
  {
    struct whc_node *cur, *last;
    uint32_t n = 0;
    for (cur = deferred_free_list, last = NULL; cur; last = cur, cur = cur->next_seq)
    {
      n++;
      if (!cur->borrowed)
        free_whc_node_contents (cur);
    }
    cur = nn_freelist_pushmany (&whc_node_freelist, deferred_free_list, last, n);
    while (cur)
    {
      struct whc_node *tmp = cur;
      cur = cur->next_seq;
      ddsrt_free (tmp);
    }
  }
}

static void whc_default_free_deferred_free_list (struct whc *whc_generic, struct whc_node *deferred_free_list)
{
  (void) whc_generic;
  free_deferred_free_list (deferred_free_list);
}

static uint32_t whc_default_remove_acked_messages_noidx (struct whc_impl *whc, seqno_t max_drop_seq, struct whc_node **deferred_free_list)
{
  struct whc_intvnode *intv;
  struct whc_node *whcn;
  uint32_t ndropped = 0;

  /* In the trivial case of an empty WHC, get out quickly */
  if (max_drop_seq <= whc->max_drop_seq || whc->maxseq_node == NULL)
  {
    if (max_drop_seq > whc->max_drop_seq)
      whc->max_drop_seq = max_drop_seq;
    *deferred_free_list = NULL;
    return 0;
  }

  /* If simple, we have always dropped everything up to whc->max_drop_seq,
   and there can only be a single interval */
#ifndef NDEBUG
  whcn = find_nextseq_intv (&intv, whc, whc->max_drop_seq);
  assert (whcn == NULL || whcn->prev_seq == NULL);
  assert (ddsrt_avl_is_singleton (&whc->seq));
#endif
  intv = whc->open_intv;

  /* Drop everything up to and including max_drop_seq, or absent that one,
   the highest available sequence number (which then must be less) */
  if ((whcn = whc_findseq (whc, max_drop_seq)) == NULL)
  {
    if (max_drop_seq < intv->min)
    {
      /* at startup, whc->max_drop_seq = 0 and reader states have max ack'd seq taken from wr->seq;
       so if multiple readers are matched and the writer runs ahead of the readers, for the first
       ack, whc->max_drop_seq < max_drop_seq = MIN (readers max ack) < intv->min */
      if (max_drop_seq > whc->max_drop_seq)
        whc->max_drop_seq = max_drop_seq;
      *deferred_free_list = NULL;
      return 0;
    }
    else
    {
      whcn = whc->maxseq_node;
      assert (whcn->seq < max_drop_seq);
    }
  }

  *deferred_free_list = intv->first;
  ndropped = (uint32_t) (whcn->seq - intv->min + 1);

  intv->first = whcn->next_seq;
  intv->min = max_drop_seq + 1;
  if (whcn->next_seq == NULL)
  {
    whc->maxseq_node = NULL;
    intv->maxp1 = intv->min;
  }
  else
  {
    assert (whcn->next_seq->seq == max_drop_seq + 1);
    whcn->next_seq->prev_seq = NULL;
  }
  whcn->next_seq = NULL;

  assert (whcn->total_bytes - (*deferred_free_list)->total_bytes + (*deferred_free_list)->size <= whc->unacked_bytes);
  whc->unacked_bytes -= (size_t) (whcn->total_bytes - (*deferred_free_list)->total_bytes + (*deferred_free_list)->size);
  for (whcn = *deferred_free_list; whcn; whcn = whcn->next_seq)
  {
#ifdef DDS_HAS_LIFESPAN
    lifespan_unregister_sample_locked (&whc->lifespan, &whcn->lifespan);
#endif
    remove_whcn_from_hash (whc, whcn);
    assert (whcn->unacked);
  }

  assert (ndropped <= whc->seq_size);
  whc->seq_size -= ndropped;
  whc->max_drop_seq = max_drop_seq;
  return ndropped;
}

static uint32_t whc_default_remove_acked_messages_full (struct whc_impl *whc, seqno_t max_drop_seq, struct whc_node **deferred_free_list)
{
  struct whc_intvnode *intv;
  struct whc_node *whcn;
  struct whc_node *prev_seq;
  struct whc_node deferred_list_head, *last_to_free = &deferred_list_head;
  uint32_t ndropped = 0;

  whcn = find_nextseq_intv (&intv, whc, whc->max_drop_seq);
  if (whc->wrinfo.is_transient_local && whc->wrinfo.tldepth == 0)
  {
    /* KEEP_ALL on transient local, so we can never ever delete anything, but
       we have to ack the data in whc */
    TRACE ("  KEEP_ALL transient-local: ack data\n");
    while (whcn && whcn->seq <= max_drop_seq)
    {
      if (whcn->unacked)
      {
        assert (whc->unacked_bytes >= whcn->size);
        whc->unacked_bytes -= whcn->size;
        whcn->unacked = 0;
      }
      whcn = whcn->next_seq;
    }
    whc->max_drop_seq = max_drop_seq;
    *deferred_free_list = NULL;
    return 0;
  }

  deferred_list_head.next_seq = NULL;
  prev_seq = whcn ? whcn->prev_seq : NULL;
  while (whcn && whcn->seq <= max_drop_seq)
  {
    TRACE ("  whcn %p %"PRIu64, (void *) whcn, whcn->seq);
    if (whcn_in_tlidx (whc, whcn->idxnode, whcn->idxnode_pos))
    {
      /* quickly skip over samples in tlidx */
      TRACE (" tl:keep");
      if (whcn->unacked)
      {
        assert (whc->unacked_bytes >= whcn->size);
        whc->unacked_bytes -= whcn->size;
        whcn->unacked = 0;
      }

      if (whcn == intv->last)
        intv = ddsrt_avl_find_succ (&whc_seq_treedef, &whc->seq, intv);
      if (prev_seq)
        prev_seq->next_seq = whcn;
      whcn->prev_seq = prev_seq;
      prev_seq = whcn;
      whcn = whcn->next_seq;
    }
    else
    {
      TRACE (" delete");
      last_to_free->next_seq = whcn;
      last_to_free = last_to_free->next_seq;
      whc_delete_one_intv (whc, &intv, &whcn);
      ndropped++;
    }
    TRACE ("\n");
  }
  if (prev_seq)
    prev_seq->next_seq = whcn;
  if (whcn)
    whcn->prev_seq = prev_seq;
  last_to_free->next_seq = NULL;
  *deferred_free_list = deferred_list_head.next_seq;

  /* If the history is deeper than durability_service.history (but not KEEP_ALL), then there
   may be old samples in this instance, samples that were retained because they were within
   the T-L history but that are not anymore. Writing new samples will eventually push these
   out, but if the difference is large and the update rate low, it may take a long time.
   Thus, we had better prune them. */
  if (whc->wrinfo.tldepth > 0 && whc->wrinfo.idxdepth > whc->wrinfo.tldepth)
  {
    assert (whc->wrinfo.hdepth == whc->wrinfo.idxdepth);
    TRACE ("  idxdepth %"PRIu32" > tldepth %"PRIu32" > 0 -- must prune\n", whc->wrinfo.idxdepth, whc->wrinfo.tldepth);

    /* Do a second pass over the sequence number range we just processed: this time we only
     encounter samples that were retained because of the transient-local durability setting
     (the rest has been dropped already) and we prune old samples in the instance */
    whcn = find_nextseq_intv (&intv, whc, whc->max_drop_seq);
    while (whcn && whcn->seq <= max_drop_seq)
    {
      struct whc_idxnode * const idxn = whcn->idxnode;
      uint32_t cnt, idx;

      TRACE ("  whcn %p %"PRIu64" idxn %p prune_seq %"PRIu64":", (void *) whcn, whcn->seq, (void *) idxn, idxn->prune_seq);

      assert (whcn_in_tlidx (whc, idxn, whcn->idxnode_pos));
      assert (idxn->prune_seq <= max_drop_seq);

      if (idxn->prune_seq == max_drop_seq)
      {
        TRACE (" already pruned\n");
        whcn = whcn->next_seq;
        continue;
      }
      idxn->prune_seq = max_drop_seq;

      idx = idxn->headidx;
      cnt = whc->wrinfo.idxdepth - whc->wrinfo.tldepth;
      while (cnt--)
      {
        struct whc_node *oldn;
        if (++idx == whc->wrinfo.idxdepth)
          idx = 0;
        if ((oldn = idxn->hist[idx]) != NULL)
        {
          /* Delete it - but this may not result in deleting the index node as
           there must still be a more recent one available */
#ifndef NDEBUG
          struct whc_idxnode template;
          template.iid = idxn->iid;
          assert (oldn->seq < whcn->seq);
#endif
          TRACE (" del %p %"PRIu64, (void *) oldn, oldn->seq);
          whc_delete_one (whc, oldn);
#ifndef NDEBUG
          assert (ddsrt_hh_lookup (whc->idx_hash, &template) == idxn);
#endif
        }
      }
      TRACE ("\n");
      whcn = whcn->next_seq;
    }
  }

  assert (ndropped <= whc->seq_size);
  whc->seq_size -= ndropped;

  /* lazy people do it this way: */
  whc->maxseq_node = whc_findmax_procedurally (whc);
  whc->max_drop_seq = max_drop_seq;
  return ndropped;
}

static uint32_t whc_default_remove_acked_messages (struct whc *whc_generic, seqno_t max_drop_seq, struct whc_state *whcst, struct whc_node **deferred_free_list)
{
  struct whc_impl * const whc = (struct whc_impl *)whc_generic;
  uint32_t cnt;

  ddsrt_mutex_lock (&whc->lock);
  assert (max_drop_seq < MAX_SEQ_NUMBER);
  assert (max_drop_seq >= whc->max_drop_seq);

  if (whc->gv->logconfig.c.mask & DDS_LC_WHC)
  {
    struct whc_state tmp;
    get_state_locked (whc, &tmp);
    TRACE ("whc_default_remove_acked_messages(%p max_drop_seq %"PRIu64")\n", (void *)whc, max_drop_seq);
    TRACE ("  whc: [%"PRIu64",%"PRIu64"] max_drop_seq %"PRIu64" h %"PRIu32" tl %"PRIu32"\n",
           tmp.min_seq, tmp.max_seq, whc->max_drop_seq, whc->wrinfo.hdepth, whc->wrinfo.tldepth);
  }

  check_whc (whc);

  /* In case a deadline is set, a sample may be added to whc temporarily, which could be
     stored in acked state. The _noidx variant of removing messages assumes that unacked
     data exists in whc. So in case of a deadline, the _full variant is used instead,
     even when index depth is 0 */
  if (whc->wrinfo.idxdepth == 0 && !whc->wrinfo.has_deadline && !whc->wrinfo.is_transient_local)
    cnt = whc_default_remove_acked_messages_noidx (whc, max_drop_seq, deferred_free_list);
  else
    cnt = whc_default_remove_acked_messages_full (whc, max_drop_seq, deferred_free_list);
  get_state_locked (whc, whcst);
  ddsrt_mutex_unlock (&whc->lock);
  return cnt;
}

static struct whc_node *whc_default_insert_seq (struct whc_impl *whc, seqno_t max_drop_seq, seqno_t seq, ddsrt_mtime_t exp, struct ddsi_plist *plist, struct ddsi_serdata *serdata)
{
  struct whc_node *newn = NULL;

#ifndef DDS_HAS_LIFESPAN
  DDSRT_UNUSED_ARG (exp);
#endif

  if ((newn = nn_freelist_pop (&whc_node_freelist)) == NULL)
    newn = ddsrt_malloc (sizeof (*newn));
  newn->seq = seq;
  newn->plist = plist;
  newn->unacked = (seq > max_drop_seq);
  newn->borrowed = 0;
  newn->idxnode = NULL; /* initial state, may be changed */
  newn->idxnode_pos = 0;
  newn->last_rexmit_ts.v = 0;
  newn->rexmit_count = 0;
  newn->serdata = ddsi_serdata_ref (serdata);
  newn->next_seq = NULL;
  newn->prev_seq = whc->maxseq_node;
  if (newn->prev_seq)
    newn->prev_seq->next_seq = newn;
  whc->maxseq_node = newn;

  newn->size = whcn_size (whc, newn);
  whc->total_bytes += newn->size;
  newn->total_bytes = whc->total_bytes;
  if (newn->unacked)
    whc->unacked_bytes += newn->size;

#ifdef DDS_HAS_LIFESPAN
  newn->lifespan.t_expire = exp;
#endif

  insert_whcn_in_hash (whc, newn);

  if (whc->open_intv->first == NULL)
  {
    /* open_intv is empty => reset open_intv */
    whc->open_intv->min = seq;
    whc->open_intv->maxp1 = seq + 1;
    whc->open_intv->first = whc->open_intv->last = newn;
  }
  else if (whc->open_intv->maxp1 == seq)
  {
    /* no gap => append to open_intv */
    whc->open_intv->last = newn;
    whc->open_intv->maxp1++;
  }
  else
  {
    /* gap => need new open_intv */
    struct whc_intvnode *intv1;
    ddsrt_avl_ipath_t path;
    intv1 = ddsrt_malloc (sizeof (*intv1));
    intv1->min = seq;
    intv1->maxp1 = seq + 1;
    intv1->first = intv1->last = newn;
    if (ddsrt_avl_lookup_ipath (&whc_seq_treedef, &whc->seq, &seq, &path) != NULL)
      assert (0);
    ddsrt_avl_insert_ipath (&whc_seq_treedef, &whc->seq, intv1, &path);
    whc->open_intv = intv1;
  }

  whc->seq_size++;
#ifdef DDS_HAS_LIFESPAN
  lifespan_register_sample_locked (&whc->lifespan, &newn->lifespan);
#endif
  return newn;
}

static int whc_default_insert (struct whc *whc_generic, seqno_t max_drop_seq, seqno_t seq, ddsrt_mtime_t exp, struct ddsi_plist *plist, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk)
{
  struct whc_impl * const whc = (struct whc_impl *)whc_generic;
  struct whc_node *newn = NULL;
  struct whc_idxnode *idxn;
  union {
    struct whc_idxnode idxn;
    char pad[sizeof (struct whc_idxnode) + sizeof (struct whc_node *)];
  } template;

  /* FIXME: the 'exp' arg is used for lifespan, refactor this parameter to a struct 'writer info'
    that contains both lifespan als deadline info of the writer */

  ddsrt_mutex_lock (&whc->lock);
  check_whc (whc);

  if (whc->gv->logconfig.c.mask & DDS_LC_WHC)
  {
    struct whc_state whcst;
    get_state_locked (whc, &whcst);
    TRACE ("whc_default_insert(%p max_drop_seq %"PRIu64" seq %"PRIu64" exp %"PRId64" plist %p serdata %p:%"PRIx32")\n",
           (void *) whc, max_drop_seq, seq, exp.v, (void *) plist, (void *) serdata, serdata->hash);
    TRACE ("  whc: [%"PRIu64",%"PRIu64"] max_drop_seq %"PRIu64" h %"PRIu32" tl %"PRIu32"\n",
           whcst.min_seq, whcst.max_seq, whc->max_drop_seq, whc->wrinfo.hdepth, whc->wrinfo.tldepth);
  }

  assert (max_drop_seq < MAX_SEQ_NUMBER);
  assert (max_drop_seq >= whc->max_drop_seq);

  /* Seq must be greater than what is currently stored. Usually it'll
   be the next sequence number, but if there are no readers
   temporarily, a gap may be among the possibilities */
  assert (whc->seq_size == 0 || seq > whc->maxseq_node->seq);

  /* Always insert in seq admin */
  newn = whc_default_insert_seq (whc, max_drop_seq, seq, exp, plist, serdata);

  TRACE ("  whcn %p:", (void*)newn);

  /* Special case of empty data (such as commit messages) can't go into index, and if we're not maintaining an index, we're done, too */
  if (serdata->kind == SDK_EMPTY)
  {
    TRACE (" empty or no hist\n");
    ddsrt_mutex_unlock (&whc->lock);
    return 0;
  }

  template.idxn.iid = tk->m_iid;
  if ((idxn = ddsrt_hh_lookup (whc->idx_hash, &template)) != NULL)
  {
    /* Unregisters cause deleting of index entry, non-unregister of adding/overwriting in history */
    TRACE (" idxn %p", (void *)idxn);
    if (serdata->statusinfo & NN_STATUSINFO_UNREGISTER)
    {
      TRACE (" unreg:delete\n");
      delete_one_instance_from_idx (whc, max_drop_seq, idxn);
      if (newn->seq <= max_drop_seq)
      {
        struct whc_node *prev_seq = newn->prev_seq;
        TRACE (" unreg:seq <= max_drop_seq: delete newn\n");
        whc_delete_one (whc, newn);
        whc->maxseq_node = prev_seq;
      }
    }
    else
    {
#ifdef DDS_HAS_DEADLINE_MISSED
      deadline_renew_instance_locked (&whc->deadline, &idxn->deadline);
#endif
      if (whc->wrinfo.idxdepth > 0)
      {
        struct whc_node *oldn;
        if (++idxn->headidx == whc->wrinfo.idxdepth)
          idxn->headidx = 0;
        if ((oldn = idxn->hist[idxn->headidx]) != NULL)
        {
          TRACE (" overwrite whcn %p", (void *)oldn);
          oldn->idxnode = NULL;
        }
        idxn->hist[idxn->headidx] = newn;
        newn->idxnode = idxn;
        newn->idxnode_pos = idxn->headidx;

        if (oldn && (whc->wrinfo.hdepth > 0 || oldn->seq <= max_drop_seq) && (!whc->wrinfo.is_transient_local || whc->wrinfo.tldepth > 0))
        {
          TRACE (" prune whcn %p", (void *)oldn);
          assert (oldn != whc->maxseq_node || whc->wrinfo.has_deadline);
          whc_delete_one (whc, oldn);
          if (oldn == whc->maxseq_node)
            whc->maxseq_node = whc_findmax_procedurally (whc);
        }

        /* Special case for dropping everything beyond T-L history when the new sample is being
        auto-acknowledged (for lack of reliable readers), and the keep-last T-L history is
        shallower than the keep-last regular history (normal path handles this via pruning in
        whc_default_remove_acked_messages, but that never happens when there are no readers). */
        if (seq <= max_drop_seq && whc->wrinfo.tldepth > 0 && whc->wrinfo.idxdepth > whc->wrinfo.tldepth)
        {
          uint32_t pos = idxn->headidx + whc->wrinfo.idxdepth - whc->wrinfo.tldepth;
          if (pos >= whc->wrinfo.idxdepth)
            pos -= whc->wrinfo.idxdepth;
          if ((oldn = idxn->hist[pos]) != NULL)
          {
            TRACE (" prune tl whcn %p", (void *)oldn);
            assert (oldn != whc->maxseq_node);
            whc_delete_one (whc, oldn);
          }
        }
        TRACE ("\n");
      }
    }
  }
  else
  {
    TRACE (" newkey");
    /* Ignore unregisters, but insert everything else */
    if (!(serdata->statusinfo & NN_STATUSINFO_UNREGISTER))
    {
      idxn = ddsrt_malloc (sizeof (*idxn) + whc->wrinfo.idxdepth * sizeof (idxn->hist[0]));
      TRACE (" idxn %p", (void *)idxn);
      ddsi_tkmap_instance_ref (tk);
      idxn->iid = tk->m_iid;
      idxn->tk = tk;
      idxn->prune_seq = 0;
      idxn->headidx = 0;
      if (whc->wrinfo.idxdepth > 0)
      {
        idxn->hist[0] = newn;
        for (uint32_t i = 1; i < whc->wrinfo.idxdepth; i++)
          idxn->hist[i] = NULL;
        newn->idxnode = idxn;
        newn->idxnode_pos = 0;
      }
      ddsrt_hh_add_absent (whc->idx_hash, idxn);
#ifdef DDS_HAS_DEADLINE_MISSED
      deadline_register_instance_locked (&whc->deadline, &idxn->deadline, ddsrt_time_monotonic ());
#endif
    }
    else
    {
      TRACE (" unreg:skip");
      if (newn->seq <= max_drop_seq)
      {
        struct whc_node *prev_seq = newn->prev_seq;
        TRACE (" unreg:seq <= max_drop_seq: delete newn\n");
        whc_delete_one (whc, newn);
        whc->maxseq_node = prev_seq;
      }
    }
    TRACE ("\n");
  }
  ddsrt_mutex_unlock (&whc->lock);
  return 0;
}

static void make_borrowed_sample (struct whc_borrowed_sample *sample, struct whc_node *whcn)
{
  assert (!whcn->borrowed);
  whcn->borrowed = 1;
  sample->seq = whcn->seq;
  sample->plist = whcn->plist;
  sample->serdata = whcn->serdata;
  sample->unacked = whcn->unacked;
  sample->rexmit_count = whcn->rexmit_count;
  sample->last_rexmit_ts = whcn->last_rexmit_ts;
}

static bool whc_default_borrow_sample (const struct whc *whc_generic, seqno_t seq, struct whc_borrowed_sample *sample)
{
  const struct whc_impl * const whc = (const struct whc_impl *)whc_generic;
  struct whc_node *whcn;
  bool found;
  ddsrt_mutex_lock ((ddsrt_mutex_t *)&whc->lock);
  if ((whcn = whc_findseq (whc, seq)) == NULL)
    found = false;
  else
  {
    make_borrowed_sample (sample, whcn);
    found = true;
  }
  ddsrt_mutex_unlock ((ddsrt_mutex_t *)&whc->lock);
  return found;
}

static bool whc_default_borrow_sample_key (const struct whc *whc_generic, const struct ddsi_serdata *serdata_key, struct whc_borrowed_sample *sample)
{
  const struct whc_impl * const whc = (const struct whc_impl *)whc_generic;
  struct whc_node *whcn;
  bool found;
  ddsrt_mutex_lock ((ddsrt_mutex_t *)&whc->lock);
  if ((whcn = whc_findkey (whc, serdata_key)) == NULL)
    found = false;
  else
  {
    make_borrowed_sample (sample, whcn);
    found = true;
  }
  ddsrt_mutex_unlock ((ddsrt_mutex_t *)&whc->lock);
  return found;
}

static void return_sample_locked (struct whc_impl *whc, struct whc_borrowed_sample *sample, bool update_retransmit_info)
{
  struct whc_node *whcn;
  if ((whcn = whc_findseq (whc, sample->seq)) == NULL)
  {
    /* data no longer present in WHC - that means ownership for serdata, plist shifted to the borrowed copy and "returning" it really becomes "destroying" it */
    ddsi_serdata_unref (sample->serdata);
    if (sample->plist)
    {
      ddsi_plist_fini (sample->plist);
      ddsrt_free (sample->plist);
    }
  }
  else
  {
    assert (whcn->borrowed);
    whcn->borrowed = 0;
    if (update_retransmit_info)
    {
      whcn->rexmit_count = sample->rexmit_count;
      whcn->last_rexmit_ts = sample->last_rexmit_ts;
    }
  }
}

static void whc_default_return_sample (struct whc *whc_generic, struct whc_borrowed_sample *sample, bool update_retransmit_info)
{
  struct whc_impl * const whc = (struct whc_impl *)whc_generic;
  ddsrt_mutex_lock (&whc->lock);
  return_sample_locked (whc, sample, update_retransmit_info);
  ddsrt_mutex_unlock (&whc->lock);
}

static void whc_default_sample_iter_init (const struct whc *whc_generic, struct whc_sample_iter *opaque_it)
{
  struct whc_sample_iter_impl *it = (struct whc_sample_iter_impl *)opaque_it;
  it->c.whc = (struct whc *)whc_generic;
  it->first = true;
}

static bool whc_default_sample_iter_borrow_next (struct whc_sample_iter *opaque_it, struct whc_borrowed_sample *sample)
{
  struct whc_sample_iter_impl * const it = (struct whc_sample_iter_impl *)opaque_it;
  struct whc_impl * const whc = (struct whc_impl *)it->c.whc;
  struct whc_node *whcn;
  struct whc_intvnode *intv;
  seqno_t seq;
  bool valid;
  ddsrt_mutex_lock (&whc->lock);
  check_whc (whc);
  if (!it->first)
  {
    seq = sample->seq;
    return_sample_locked (whc, sample, false);
  }
  else
  {
    it->first = false;
    seq = 0;
  }
  if ((whcn = find_nextseq_intv (&intv, whc, seq)) == NULL)
    valid = false;
  else
  {
    make_borrowed_sample (sample, whcn);
    valid = true;
  }
  ddsrt_mutex_unlock (&whc->lock);
  return valid;
}
