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

#include "os/os.h"
#include "ddsi/ddsi_ser.h"
#include "ddsi/q_unused.h"
#include "ddsi/q_config.h"
#include "ddsi/q_whc.h"
#include "q__osplser.h"
#include "dds__tkmap.h"

/* Avoiding all nn_log-related activities when LC_WHC is not set
   (and it hardly ever is, as it is not even included in "trace")
   saves a couple of % CPU on a high-rate publisher - that's worth
   it.  So we need a macro & a support function. */
static int trace_whc (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  nn_vlog (LC_WHC, fmt, ap);
  va_end (ap);
  return 0;
}
#define TRACE_WHC(args) ((config.enabled_logcats & LC_WHC) ? (trace_whc args) : 0)

/* Hash + interval tree adminitration of samples-by-sequence number
 * - by definition contains all samples in WHC (unchanged from older versions)
 * Circular array of samples per instance, inited to all 0
 * - length is max(durability_service.history_depth, history.depth), KEEP_ALL => as-if 0
 * - no instance index if above length 0
 * - each sample (i.e., whc_node): backpointer into index
 * - maintain index of latest sample, end of history then trivially follows from index arithmetic
 * Overwriting in insert drops them from index, depending on "aggressiveness" from by-seq
 * - special case for no readers (i.e. no ACKs) and history > transient-local history
 * - cleaning up after ACKs has additional pruning stage for same case
 */

static void insert_whcn_in_hash (struct whc *whc, struct whc_node *whcn);
static void whc_delete_one (struct whc *whc, struct whc_node *whcn);
static int compare_seq (const void *va, const void *vb);
static unsigned whc_remove_acked_messages_full (struct whc *whc, seqno_t max_drop_seq, struct whc_node **deferred_free_list);

static const ut_avlTreedef_t whc_seq_treedef =
  UT_AVL_TREEDEF_INITIALIZER (offsetof (struct whc_intvnode, avlnode), offsetof (struct whc_intvnode, min), compare_seq, 0);

#if USE_EHH
static uint32_t whc_seq_entry_hash (const void *vn)
{
  const struct whc_seq_entry *n = vn;
  /* we hash the lower 32 bits, on the assumption that with 4 billion
   samples in between there won't be significant correlation */
  const uint64_t c = UINT64_C(16292676669999574021);
  const uint32_t x = (uint32_t) n->seq;
  return (unsigned) ((x * c) >> 32);
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
  const uint64_t c = UINT64_C(16292676669999574021);
  const uint32_t x = (uint32_t) n->seq;
  return (unsigned) ((x * c) >> 32);
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

static struct whc_node *whc_findmax_procedurally (const struct whc *whc)
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
    struct whc_intvnode *intv = ut_avlFindPred (&whc_seq_treedef, &whc->seq, whc->open_intv);
    assert (intv && intv->first);
    return intv->last;
  }
}

static void check_whc (const struct whc *whc)
{
  /* there's much more we can check, but it gets expensive quite
     quickly: all nodes but open_intv non-empty, non-overlapping and
     non-contiguous; min & maxp1 of intervals correct; each interval
     contiguous; all samples in seq & in seqhash; tlidx \subseteq seq;
     seq-number ordered list correct; &c. */
  assert (whc->open_intv != NULL);
  assert (whc->open_intv == ut_avlFindMax (&whc_seq_treedef, &whc->seq));
  assert (ut_avlFindSucc (&whc_seq_treedef, &whc->seq, whc->open_intv) == NULL);
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

#if 1 && !defined(NDEBUG)
  {
    struct whc_intvnode *firstintv;
    struct whc_node *cur;
    seqno_t prevseq = 0;
    firstintv = ut_avlFindMin (&whc_seq_treedef, &whc->seq);
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

static void insert_whcn_in_hash (struct whc *whc, struct whc_node *whcn)
{
  /* precondition: whcn is not in hash */
#if USE_EHH
  struct whc_seq_entry e = { .seq = whcn->seq, .whcn = whcn };
  if (!ut_ehhAdd (whc->seq_hash, &e))
    assert(0);
#else
  if (!ut_hhAdd (whc->seq_hash, whcn))
    assert(0);
#endif
}

static void remove_whcn_from_hash (struct whc *whc, struct whc_node *whcn)
{
  /* precondition: whcn is in hash */
#if USE_EHH
  struct whc_seq_entry e = { .seq = whcn->seq };
  if (!ut_ehhRemove(whc->seq_hash, &e))
    assert(0);
#else
  if (!ut_hhRemove(whc->seq_hash, whcn))
    assert(0);
#endif
}

struct whc_node *whc_findseq (const struct whc *whc, seqno_t seq)
{
#if USE_EHH
  struct whc_seq_entry e = { .seq = seq }, *r;
  if ((r = ut_ehhLookup(whc->seq_hash, &e)) != NULL)
    return r->whcn;
  else
    return NULL;
#else
  struct whc_node template;
  template.seq = seq;
  return ut_hhLookup(whc->seq_hash, &template);
#endif
}


struct whc *whc_new (int is_transient_local, unsigned hdepth, unsigned tldepth, size_t sample_overhead)
{
  struct whc *whc;
  struct whc_intvnode *intv;

  assert((hdepth == 0 || tldepth <= hdepth) || is_transient_local);

  whc = os_malloc (sizeof (*whc));
  whc->is_transient_local = is_transient_local ? 1 : 0;
  whc->hdepth = hdepth;
  whc->tldepth = tldepth;
  whc->idxdepth = hdepth > tldepth ? hdepth : tldepth;
  whc->seq_size = 0;
  whc->max_drop_seq = 0;
  whc->unacked_bytes = 0;
  whc->total_bytes = 0;
  whc->sample_overhead = sample_overhead;
#if USE_EHH
  whc->seq_hash = ut_ehhNew (sizeof (struct whc_seq_entry), 32, whc_seq_entry_hash, whc_seq_entry_eq);
#else
  whc->seq_hash = ut_hhNew(32, whc_node_hash, whc_node_eq);
#endif

  if (whc->idxdepth > 0)
    whc->idx_hash = ut_hhNew(32, whc_idxnode_hash_key, whc_idxnode_eq_key);
  else
    whc->idx_hash = NULL;

  /* seq interval tree: always has an "open" node */
  ut_avlInit (&whc_seq_treedef, &whc->seq);
  intv = os_malloc (sizeof (*intv));
  intv->min = intv->maxp1 = 1;
  intv->first = intv->last = NULL;
  ut_avlInsert (&whc_seq_treedef, &whc->seq, intv);
  whc->open_intv = intv;
  whc->maxseq_node = NULL;

  /* hack */
  nn_freelist_init (&whc->freelist, UINT32_MAX, offsetof (struct whc_node, next_seq));

  check_whc (whc);
  return whc;
}

static void free_whc_node_contents (struct whc *whc, struct whc_node *whcn)
{
  ddsi_serdata_unref (whcn->serdata);
  if (whcn->plist) {
    nn_plist_fini (whcn->plist);
    os_free (whcn->plist);
  }
}

void whc_free (struct whc *whc)
{
  /* Freeing stuff without regards for maintaining data structures */
  check_whc (whc);

  if (whc->idx_hash)
  {
    struct ut_hhIter it;
    struct whc_idxnode *n;
    for (n = ut_hhIterFirst(whc->idx_hash, &it); n != NULL; n = ut_hhIterNext(&it))
      os_free(n);
    ut_hhFree(whc->idx_hash);
  }

  {
    struct whc_node *whcn = whc->maxseq_node;
    while (whcn)
    {
      struct whc_node *tmp = whcn;
/* The compiler doesn't realize that whcn->prev_seq is always initialized. */
OS_WARNING_MSVC_OFF(6001);
      whcn = whcn->prev_seq;
OS_WARNING_MSVC_ON(6001);
      free_whc_node_contents (whc, tmp);
      os_free (tmp);
    }
  }

  ut_avlFree (&whc_seq_treedef, &whc->seq, os_free);
  nn_freelist_fini (&whc->freelist, os_free);

#if USE_EHH
  ut_ehhFree (whc->seq_hash);
#else
  ut_hhFree (whc->seq_hash);
#endif
  os_free (whc);
}

int whc_empty (const struct whc *whc)
{
  return whc->seq_size == 0;
}

seqno_t whc_min_seq (const struct whc *whc)
{
  /* precond: whc not empty */
  const struct whc_intvnode *intv;
  check_whc (whc);
  assert (!whc_empty (whc));
  intv = ut_avlFindMin (&whc_seq_treedef, &whc->seq);
  assert (intv);
  /* not empty, open node may be anything but is (by definition)
     findmax, and whc is claimed to be non-empty, so min interval
     can't be empty */
  assert (intv->maxp1 > intv->min);
  return intv->min;
}

struct whc_node *whc_findmax (const struct whc *whc)
{
  check_whc (whc);
  return (struct whc_node *) whc->maxseq_node;
}

seqno_t whc_max_seq (const struct whc *whc)
{
  /* precond: whc not empty */
  check_whc (whc);
  assert (!whc_empty (whc));
  assert (whc->maxseq_node != NULL);
  return whc->maxseq_node->seq;
}

static struct whc_node *find_nextseq_intv (struct whc_intvnode **p_intv, const struct whc *whc, seqno_t seq)
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
      struct whc_intvnode *predintv = ut_avlLookupPredEq (&whc_seq_treedef, &whc->seq, &seq);
      assert (predintv == NULL || predintv->maxp1 <= seq);
    }
#endif
    if ((intv = ut_avlLookupSuccEq (&whc_seq_treedef, &whc->seq, &seq)) == NULL) {
      assert (ut_avlLookupPredEq (&whc_seq_treedef, &whc->seq, &seq) == whc->open_intv);
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
    *p_intv = ut_avlLookupPredEq (&whc_seq_treedef, &whc->seq, &n->seq);
    return n;
  }
}

seqno_t whc_next_seq (const struct whc *whc, seqno_t seq)
{
  struct whc_node *n;
  struct whc_intvnode *intv;
  check_whc (whc);
  if ((n = find_nextseq_intv (&intv, whc, seq)) == NULL)
    return MAX_SEQ_NUMBER;
  else
    return n->seq;
}

struct whc_node* whc_next_node(const struct whc *whc, seqno_t seq)
{
    struct whc_intvnode *intv;
    check_whc (whc);
    return find_nextseq_intv(&intv, whc, seq);
}

static void delete_one_sample_from_idx (struct whc *whc, struct whc_node *whcn)
{
  struct whc_idxnode * const idxn = whcn->idxnode;
  assert (idxn != NULL);
  assert (idxn->hist[idxn->headidx] != NULL);
  assert (idxn->hist[whcn->idxnode_pos] == whcn);
  if (whcn->idxnode_pos != idxn->headidx)
    idxn->hist[whcn->idxnode_pos] = NULL;
  else
  {
#ifndef NDEBUG
    unsigned i;
    for (i = 0; i < whc->idxdepth; i++)
      assert (i == idxn->headidx || idxn->hist[i] == NULL);
#endif
    if (!ut_hhRemove (whc->idx_hash, idxn))
      assert (0);
    dds_tkmap_instance_unref(idxn->tk);
    os_free (idxn);
  }
  whcn->idxnode = NULL;
}

static void free_one_instance_from_idx (struct whc *whc, seqno_t max_drop_seq, struct whc_idxnode *idxn)
{
  unsigned i;
  for (i = 0; i < whc->idxdepth; i++)
  {
    if (idxn->hist[i])
    {
      struct whc_node *oldn = idxn->hist[i];
      oldn->idxnode = NULL;
      if (oldn->seq <= max_drop_seq)
      {
        TRACE_WHC(("  prune tl whcn %p\n", (void *)oldn));
        assert(oldn != whc->maxseq_node);
        whc_delete_one (whc, oldn);
      }
    }
  }
  os_free(idxn);
}

static void delete_one_instance_from_idx (struct whc *whc, seqno_t max_drop_seq, struct whc_idxnode *idxn)
{
  if (!ut_hhRemove (whc->idx_hash, idxn))
    assert (0);
  free_one_instance_from_idx (whc, max_drop_seq, idxn);
}

static int whcn_in_tlidx (const struct whc *whc, const struct whc_idxnode *idxn, unsigned pos)
{
  if (idxn == NULL)
    return 0;
  else
  {
    unsigned d = (idxn->headidx + (pos > idxn->headidx ? whc->idxdepth : 0)) - pos;
    assert (d < whc->idxdepth);
    return d < whc->tldepth;
  }
}

void whc_downgrade_to_volatile (struct whc *whc)
{
  seqno_t old_max_drop_seq;
  struct whc_node *deferred_free_list;

  /* We only remove them from whc->tlidx: we don't remove them from
     whc->seq yet.  That'll happen eventually.  */
  check_whc (whc);

  if (whc->idxdepth == 0)
  {
    /* if not maintaining an index at all, this is nonsense */
    return;
  }

  assert (!whc->is_transient_local);
  if (whc->tldepth > 0)
  {
    assert(whc->hdepth == 0 || whc->tldepth <= whc->hdepth);
    whc->tldepth = 0;
    if (whc->hdepth == 0)
    {
      struct ut_hhIter it;
      struct whc_idxnode *n;
      for (n = ut_hhIterFirst(whc->idx_hash, &it); n != NULL; n = ut_hhIterNext(&it))
        free_one_instance_from_idx (whc, 0, n);
      ut_hhFree(whc->idx_hash);
      whc->idxdepth = 0;
      whc->idx_hash = NULL;
    }
  }

  /* Immediately drop them from the WHC (used to delay it until the
     next ack); but need to make sure remove_acked_messages processes
     them all. */
  old_max_drop_seq = whc->max_drop_seq;
  whc->max_drop_seq = 0;
  whc_remove_acked_messages_full (whc, old_max_drop_seq, &deferred_free_list);
  whc_free_deferred_free_list (whc, deferred_free_list);
  assert (whc->max_drop_seq == old_max_drop_seq);
}

static size_t whcn_size (const struct whc *whc, const struct whc_node *whcn)
{
  size_t sz = ddsi_serdata_size (whcn->serdata);
  return sz + ((sz + config.fragment_size - 1) / config.fragment_size) * whc->sample_overhead;
}

static void whc_delete_one_intv (struct whc *whc, struct whc_intvnode **p_intv, struct whc_node **p_whcn)
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
    delete_one_sample_from_idx (whc, whcn);
  if (whcn->unacked)
  {
    assert (whc->unacked_bytes >= whcn->size);
    whc->unacked_bytes -= whcn->size;
    whcn->unacked = 0;
  }

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
      *p_intv = ut_avlFindSucc (&whc_seq_treedef, &whc->seq, intv);
      /* only sample in interval and not the open interval => delete interval */
      ut_avlDelete (&whc_seq_treedef, &whc->seq, tmp);
      os_free (tmp);
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
    *p_intv = ut_avlFindSucc (&whc_seq_treedef, &whc->seq, intv);
  }
  else
  {
    /* somewhere in the middle => split the interval (ideally,
       would split it lazily, but it really is a transient-local
       issue only, and so we can (for now) get away with splitting
       it greedily */
    struct whc_intvnode *new_intv;
    ut_avlIPath_t path;

    new_intv = os_malloc (sizeof (*new_intv));

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
    if (ut_avlLookupIPath (&whc_seq_treedef, &whc->seq, &new_intv->min, &path) != NULL)
      assert (0);
    ut_avlInsertIPath (&whc_seq_treedef, &whc->seq, new_intv, &path);

    if (intv == whc->open_intv)
      whc->open_intv = new_intv;
    *p_intv = new_intv;
  }
}

static void whc_delete_one (struct whc *whc, struct whc_node *whcn)
{
  struct whc_intvnode *intv;
  struct whc_node *whcn_tmp = whcn;
  intv = ut_avlLookupPredEq (&whc_seq_treedef, &whc->seq, &whcn->seq);
  assert (intv != NULL);
  whc_delete_one_intv (whc, &intv, &whcn);
  if (whcn_tmp->prev_seq)
    whcn_tmp->prev_seq->next_seq = whcn_tmp->next_seq;
  if (whcn_tmp->next_seq)
    whcn_tmp->next_seq->prev_seq = whcn_tmp->prev_seq;
  whcn_tmp->next_seq = NULL;
  whc_free_deferred_free_list (whc, whcn_tmp);
  whc->seq_size--;
}

void whc_free_deferred_free_list (struct whc *whc, struct whc_node *deferred_free_list)
{
  if (deferred_free_list)
  {
    struct whc_node *cur, *last;
    uint32_t n = 0;
    for (cur = deferred_free_list, last = NULL; cur; last = cur, cur = cur->next_seq)
    {
      n++;
      free_whc_node_contents (whc, cur);
    }
    cur = nn_freelist_pushmany (&whc->freelist, deferred_free_list, last, n);
    while (cur)
    {
      struct whc_node *tmp = cur;
      cur = cur->next_seq;
      os_free (tmp);
    }
  }
}

static unsigned whc_remove_acked_messages_noidx (struct whc *whc, seqno_t max_drop_seq, struct whc_node **deferred_free_list)
{
  struct whc_intvnode *intv;
  struct whc_node *whcn;
  unsigned ndropped = 0;

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
  assert (ut_avlIsSingleton (&whc->seq));
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
         ack, whc->max_drop_seq < max_drop_seq = MIN(readers max ack) < intv->min */
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
  ndropped = (unsigned) (whcn->seq - intv->min + 1);

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
    remove_whcn_from_hash (whc, whcn);
    assert (whcn->unacked);
  }

  assert (ndropped <= whc->seq_size);
  whc->seq_size -= ndropped;
  whc->max_drop_seq = max_drop_seq;
  return ndropped;
}

static unsigned whc_remove_acked_messages_full (struct whc *whc, seqno_t max_drop_seq, struct whc_node **deferred_free_list)
{
  struct whc_intvnode *intv;
  struct whc_node *whcn;
  struct whc_node *prev_seq;
  struct whc_node deferred_list_head, *last_to_free = &deferred_list_head;
  unsigned ndropped = 0;

  if (whc->is_transient_local && whc->tldepth == 0)
  {
    /* KEEP_ALL on transient local, so we can never ever delete anything */
    TRACE_WHC(("  KEEP_ALL transient-local: do nothing\n"));
    *deferred_free_list = NULL;
    return 0;
  }

  whcn = find_nextseq_intv (&intv, whc, whc->max_drop_seq);
  deferred_list_head.next_seq = NULL;
  prev_seq = whcn ? whcn->prev_seq : NULL;
  while (whcn && whcn->seq <= max_drop_seq)
  {
    TRACE_WHC(("  whcn %p %"PRId64, (void *) whcn, whcn->seq));
    if (whcn_in_tlidx(whc, whcn->idxnode, whcn->idxnode_pos))
    {
      /* quickly skip over samples in tlidx */
      TRACE_WHC((" tl:keep"));
      if (whcn->unacked)
      {
        assert (whc->unacked_bytes >= whcn->size);
        whc->unacked_bytes -= whcn->size;
        whcn->unacked = 0;
      }

      if (whcn == intv->last)
        intv = ut_avlFindSucc (&whc_seq_treedef, &whc->seq, intv);
      if (prev_seq)
        prev_seq->next_seq = whcn;
      whcn->prev_seq = prev_seq;
      prev_seq = whcn;
      whcn = whcn->next_seq;
    }
    else
    {
      TRACE_WHC((" delete"));
      last_to_free->next_seq = whcn;
      last_to_free = last_to_free->next_seq;
      whc_delete_one_intv (whc, &intv, &whcn);
      ndropped++;
    }
    TRACE_WHC(("\n"));
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
  if (whc->tldepth > 0 && whc->idxdepth > whc->tldepth)
  {
    assert(whc->hdepth == whc->idxdepth);
    TRACE_WHC(("  idxdepth %u > tldepth %u > 0 -- must prune\n", whc->idxdepth, whc->tldepth));

    /* Do a second pass over the sequence number range we just processed: this time we only
       encounter samples that were retained because of the transient-local durability setting
       (the rest has been dropped already) and we prune old samples in the instance */
    whcn = find_nextseq_intv (&intv, whc, whc->max_drop_seq);
    while (whcn && whcn->seq <= max_drop_seq)
    {
      struct whc_idxnode * const idxn = whcn->idxnode;
      unsigned cnt, idx;

      TRACE_WHC(("  whcn %p %"PRId64" idxn %p prune_seq %"PRId64":", (void *)whcn, whcn->seq, (void *)idxn, idxn->prune_seq));

      assert(whcn_in_tlidx(whc, idxn, whcn->idxnode_pos));
      assert (idxn->prune_seq <= max_drop_seq);

      if (idxn->prune_seq == max_drop_seq)
      {
        TRACE_WHC((" already pruned\n"));
        whcn = whcn->next_seq;
        continue;
      }
      idxn->prune_seq = max_drop_seq;

      idx = idxn->headidx;
      cnt = whc->idxdepth - whc->tldepth;
      while (cnt--)
      {
        struct whc_node *oldn;
        if (++idx == whc->idxdepth)
          idx = 0;
        if ((oldn = idxn->hist[idx]) != NULL)
        {
          /* Delete it - but this may not result in deleting the index node as
             there must still be a more recent one available */
#ifndef NDEBUG
          struct whc_node whcn_template;
          union {
            struct whc_idxnode idxn;
            char pad[sizeof(struct whc_idxnode) + sizeof(struct whc_node *)];
          } template;
          template.idxn.headidx = 0;
          template.idxn.hist[0] = &whcn_template;
          whcn_template.serdata = ddsi_serdata_ref(oldn->serdata);
          assert(oldn->seq < whcn->seq);
#endif
          TRACE_WHC((" del %p %"PRId64, (void *) oldn, oldn->seq));
          whc_delete_one (whc, oldn);
#ifndef NDEBUG
          assert(ut_hhLookup(whc->idx_hash, &template) == idxn);
          ddsi_serdata_unref(whcn_template.serdata);
#endif
        }
      }
      TRACE_WHC(("\n"));
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

unsigned whc_remove_acked_messages (struct whc *whc, seqno_t max_drop_seq, struct whc_node **deferred_free_list)
{
  assert (max_drop_seq < MAX_SEQ_NUMBER);
  assert (max_drop_seq >= whc->max_drop_seq);

  TRACE_WHC(("whc_remove_acked_messages(%p max_drop_seq %"PRId64")\n", (void *)whc, max_drop_seq));
  TRACE_WHC(("  whc: [%"PRId64",%"PRId64"] max_drop_seq %"PRId64" h %u tl %u\n",
             whc_empty(whc) ? (seqno_t)-1 : whc_min_seq(whc),
             whc_empty(whc) ? (seqno_t)-1 : whc_max_seq(whc),
             whc->max_drop_seq, whc->hdepth, whc->tldepth));

  check_whc (whc);

  if (whc->idxdepth == 0)
  {
    return whc_remove_acked_messages_noidx (whc, max_drop_seq, deferred_free_list);
  }
  else
  {
    return whc_remove_acked_messages_full (whc, max_drop_seq, deferred_free_list);
  }
}

struct whc_node *whc_findkey (const struct whc *whc, const struct serdata *serdata_key)
{
  union {
    struct whc_idxnode idxn;
    char pad[sizeof(struct whc_idxnode) + sizeof(struct whc_node *)];
  } template;
  struct whc_idxnode *n;
  check_whc (whc);
  template.idxn.iid = dds_tkmap_lookup(gv.m_tkmap, serdata_key);
  n = ut_hhLookup (whc->idx_hash, &template.idxn);
  if (n == NULL)
    return NULL;
  else
  {
    assert (n->hist[n->headidx]);
    return n->hist[n->headidx];
  }
}

static struct whc_node *whc_insert_seq (struct whc *whc, seqno_t max_drop_seq, seqno_t seq, struct nn_plist *plist, serdata_t serdata)
{
  struct whc_node *newn = NULL;

  if ((newn = nn_freelist_pop (&whc->freelist)) == NULL)
    newn = os_malloc (sizeof (*newn));
  newn->seq = seq;
  newn->plist = plist;
  newn->unacked = (seq > max_drop_seq);
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
    ut_avlIPath_t path;
    intv1 = os_malloc (sizeof (*intv1));
    intv1->min = seq;
    intv1->maxp1 = seq + 1;
    intv1->first = intv1->last = newn;
    if (ut_avlLookupIPath (&whc_seq_treedef, &whc->seq, &seq, &path) != NULL)
      assert (0);
    ut_avlInsertIPath (&whc_seq_treedef, &whc->seq, intv1, &path);
    whc->open_intv = intv1;
  }

  whc->seq_size++;
  return newn;
}

int whc_insert (struct whc *whc, seqno_t max_drop_seq, seqno_t seq, struct nn_plist *plist, serdata_t serdata, struct tkmap_instance *tk)
{
  struct whc_node *newn = NULL;
  struct whc_idxnode *idxn;
  union {
    struct whc_idxnode idxn;
    char pad[sizeof(struct whc_idxnode) + sizeof(struct whc_node *)];
  } template;
  check_whc (whc);

  TRACE_WHC(("whc_insert(%p max_drop_seq %"PRId64" seq %"PRId64" plist %p serdata %p:%x)\n", (void *)whc, max_drop_seq, seq, (void*)plist, (void*)serdata, *(unsigned *)serdata->v.keyhash.m_hash));
  TRACE_WHC(("  whc: [%"PRId64",%"PRId64"] max_drop_seq %"PRId64" h %u tl %u\n",
             whc_empty(whc) ? (seqno_t)-1 : whc_min_seq(whc),
             whc_empty(whc) ? (seqno_t)-1 : whc_max_seq(whc),
             whc->max_drop_seq, whc->hdepth, whc->tldepth));

  assert (max_drop_seq < MAX_SEQ_NUMBER);
  assert (max_drop_seq >= whc->max_drop_seq);

  /* Seq must be greater than what is currently stored. Usually it'll
     be the next sequence number, but if there are no readers
     temporarily, a gap may be among the possibilities */
  assert (whc_empty (whc) || seq > whc_max_seq (whc));

  /* Always insert in seq admin */
  newn = whc_insert_seq (whc, max_drop_seq, seq, plist, serdata);

  TRACE_WHC(("  whcn %p:", (void*)newn));

  /* Special case of empty data (such as commit messages) can't go into index, and if we're not maintaining an index, we're done, too */
  if (ddsi_serdata_is_empty(serdata) || whc->idxdepth == 0)
  {
    TRACE_WHC((" empty or no hist\n"));
    return 0;
  }

  template.idxn.iid = tk->m_iid;
  if ((idxn = ut_hhLookup (whc->idx_hash, &template)) != NULL)
  {
    /* Unregisters cause deleting of index entry, non-unregister of adding/overwriting in history */
    TRACE_WHC((" idxn %p", (void *)idxn));
    if (serdata->v.msginfo.statusinfo & NN_STATUSINFO_UNREGISTER)
    {
      TRACE_WHC((" unreg:delete\n"));
      delete_one_instance_from_idx (whc, max_drop_seq, idxn);
      if (newn->seq <= max_drop_seq)
      {
        struct whc_node *prev_seq = newn->prev_seq;
        TRACE_WHC((" unreg:seq <= max_drop_seq: delete newn\n"));
        whc_delete_one (whc, newn);
        whc->maxseq_node = prev_seq;
      }
    }
    else
    {
      struct whc_node *oldn;
      if (++idxn->headidx == whc->idxdepth)
        idxn->headidx = 0;
      if ((oldn = idxn->hist[idxn->headidx]) != NULL)
      {
        TRACE_WHC((" overwrite whcn %p", (void *)oldn));
        oldn->idxnode = NULL;
      }
      idxn->hist[idxn->headidx] = newn;
      newn->idxnode = idxn;
      newn->idxnode_pos = idxn->headidx;

      if (oldn && (whc->hdepth > 0 || oldn->seq <= max_drop_seq))
      {
        TRACE_WHC((" prune whcn %p", (void *)oldn));
        assert(oldn != whc->maxseq_node);
        whc_delete_one (whc, oldn);
      }

      /* Special case for dropping everything beyond T-L history when the new sample is being
         auto-acknowledged (for lack of reliable readers), and the keep-last T-L history is
         shallower than the keep-last regular history (normal path handles this via pruning in
         whc_remove_acked_messages, but that never happens when there are no readers). */
      if (seq <= max_drop_seq && whc->tldepth > 0 && whc->idxdepth > whc->tldepth)
      {
        unsigned pos = idxn->headidx + whc->idxdepth - whc->tldepth;
        if (pos >= whc->idxdepth)
          pos -= whc->idxdepth;
        if ((oldn = idxn->hist[pos]) != NULL)
        {
          TRACE_WHC((" prune tl whcn %p", (void *)oldn));
          assert(oldn != whc->maxseq_node);
          whc_delete_one (whc, oldn);
        }
      }
      TRACE_WHC(("\n"));
    }
  }
  else
  {
    TRACE_WHC((" newkey"));
    /* Ignore unregisters, but insert everything else */
    if (!(serdata->v.msginfo.statusinfo & NN_STATUSINFO_UNREGISTER))
    {
      unsigned i;
      idxn = os_malloc (sizeof (*idxn) + whc->idxdepth * sizeof (idxn->hist[0]));
      TRACE_WHC((" idxn %p", (void *)idxn));
      dds_tkmap_instance_ref(tk);
      idxn->iid = tk->m_iid;
      idxn->tk = tk;
      idxn->prune_seq = 0;
      idxn->headidx = 0;
      idxn->hist[0] = newn;
      for (i = 1; i < whc->idxdepth; i++)
        idxn->hist[i] = NULL;
      newn->idxnode = idxn;
      newn->idxnode_pos = 0;
      if (!ut_hhAdd (whc->idx_hash, idxn))
        assert (0);
    }
    else
    {
      TRACE_WHC((" unreg:skip"));
      if (newn->seq <= max_drop_seq)
      {
        struct whc_node *prev_seq = newn->prev_seq;
        TRACE_WHC((" unreg:seq <= max_drop_seq: delete newn\n"));
        whc_delete_one (whc, newn);
        whc->maxseq_node = prev_seq;
      }
    }
    TRACE_WHC(("\n"));
  }
  return 0;
}

size_t whc_unacked_bytes (struct whc *whc)
{
  return whc->unacked_bytes;
}
