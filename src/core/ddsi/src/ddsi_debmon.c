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
#include <stddef.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "ddsi__entity.h"
#include "ddsi__endpoint_match.h"
#include "ddsi__participant.h"
#include "ddsi__misc.h"
#include "ddsi__entity_index.h"
#include "ddsi__addrset.h"
#include "ddsi__radmin.h"
#include "ddsi__discovery.h"
#include "ddsi__protocol.h"
#include "ddsi__debmon.h"
#include "ddsi__tran.h"
#include "ddsi__tcp.h"
#include "ddsi__endpoint.h"
#include "ddsi__proxy_endpoint.h"

#include "dds__whc.h"

struct ddsi_debug_monitor {
  struct ddsi_thread_state *servts;
  struct ddsi_tran_factory * tran_factory;
  struct ddsi_tran_listener * servsock;
  ddsi_locator_t servlocator;
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  struct ddsi_domaingv *gv;
  int stop;
};

struct st {
  struct ddsi_tran_conn * conn;
  struct ddsi_domaingv *gv;
  struct ddsi_thread_state *thrst;
  bool error;
  const char *comma;
  // almost guaranteed to be large enough (some strings are user-controlled),
  // everything else is way smaller than this
  char chunkbuf[4096];
  uint16_t pos;
};

static void cpemitchunk(struct st *st, ddsi_locator_t loc)
{
  // HTTP chunk delimiter (pos-8 == chunk_size)
  // 'chunk header' 2* '\r\n' + 4 hex digits = 8 bytes
  // use buf+memcpy to avoid null terminator
  char header[11];
  snprintf(header, sizeof (header), "\r\n%04"PRIX16"\r\n\r\n", (uint16_t)(st->pos - 8));
  memcpy(st->chunkbuf, header, (st->pos > 8) ? 8 : 10);

  ddsrt_iovec_t iov;
  iov.iov_base = st->chunkbuf;
  iov.iov_len = (ddsrt_iov_len_t) ((st->pos > 8) ? st->pos : 10);

  if (ddsi_conn_write (st->conn, &loc, 1, &iov, 0) < 0)
    st->error = true;

  st->pos = 8;
}

static void cpf (struct st *st, const char *fmt, ...) ddsrt_attribute_format_printf(2, 3);

static void cpf (struct st *st, const char *fmt, ...)
{
  ddsi_locator_t loc;
  if (st->error)
  {
    // do nothing
  }
  else if (!ddsi_conn_peer_locator (st->conn, &loc))
  {
    st->error = true;
  }
  else
  {
    va_list ap;
    va_start (ap, fmt);
    const int cnt = vsnprintf (st->chunkbuf + st->pos, sizeof (st->chunkbuf) - st->pos, fmt, ap);
    if (cnt < 0 || cnt > UINT16_MAX - st->pos)
      st->error = true;
    else
      st->pos = (uint16_t) (st->pos + cnt);
    va_end (ap);

    if (st->pos > sizeof (st->chunkbuf) / 2)
      cpemitchunk(st, loc);
  }
}

static void cpfstr (struct st *st, const char *v)
{
  cpf (st, "%s\"%s\"", st->comma, v);
  st->comma = ",";
}

static void cpfguid (struct st *st, const ddsi_guid_t *v)
{
  cpf (st, "%s\""PGUIDFMT"\"", st->comma, PGUID (*v));
  st->comma = ",";
}

static void cpfkstr (struct st *st, const char *key, const char *v)
{
  cpf (st, "%s\"%s\":\"%s\"", st->comma, key, v);
  st->comma = ",";
}

static void cpfkguid (struct st *st, const char *key, const ddsi_guid_t *v)
{
  cpf (st, "%s\"%s\":\""PGUIDFMT"\"", st->comma, key, PGUID (*v));
  st->comma = ",";
}

static void cpfku64 (struct st *st, const char *key, uint64_t v)
{
  cpf (st, "%s\"%s\":%"PRIu64, st->comma, key, v);
  st->comma = ",";
}

static void cpfkseqno (struct st *st, const char *key, ddsi_seqno_t v)
{
  cpfku64 (st, key, v);
}

static void cpfksize (struct st *st, const char *key, size_t v)
{
  cpf (st, "%s\"%s\":%"PRIuSIZE, st->comma, key, v);
  st->comma = ",";
}

static void cpfku32 (struct st *st, const char *key, uint32_t v)
{
  cpf (st, "%s\"%s\":%"PRIu32, st->comma, key, v);
  st->comma = ",";
}

static void cpfki64 (struct st *st, const char *key, int64_t v)
{
  cpf (st, "%s\"%s\":%"PRId64, st->comma, key, v);
  st->comma = ",";
}

static void cpfkbool (struct st *st, const char *key, bool v)
{
  cpf (st, "%s\"%s\":%s", st->comma, key, v ? "true" : "false");
  st->comma = ",";
}

static void cpfobj (struct st *st, void (*f) (struct st *st, void *v), void *v)
{
  cpf (st, "%s{", st->comma);
  st->comma = "";
  f (st, v);
  cpf (st, "}");
  st->comma = ",";
}

static void cpfkobj (struct st *st, const char *key, void (*f) (struct st *st, void *v), void *v)
{
  cpf (st, "%s\"%s\":", st->comma, key);
  st->comma = "";
  cpfobj (st, f, v);
}

static void cpfkseq (struct st *st, const char *key, void (*f) (struct st *st, void *v), void *v)
{
  cpf (st, "%s\"%s\":[", st->comma, key);
  st->comma = "";
  f (st, v);
  cpf (st, "]");
  st->comma = ",";
}

struct print_address_arg {
  struct st *st;
};

static void print_address (const ddsi_xlocator_t *n, void *varg)
{
  struct print_address_arg *arg = varg;
  char buf[DDSI_LOCSTRLEN];
  cpfstr (arg->st, ddsi_locator_to_string (buf, sizeof (buf), &n->c));
}

static void print_addrset (struct st *st, void *vas)
{
  struct ddsi_addrset * const as = vas;
  struct print_address_arg pa_arg;
  pa_arg.st = st;
  ddsi_addrset_forall (as, print_address, &pa_arg);
}

static void print_partition_seq (struct st *st, void *vxqos)
{
  dds_qos_t * const xqos = vxqos;
  if (xqos->present & DDSI_QP_PARTITION)
    for (uint32_t i = 0; i < xqos->partition.n; i++)
      cpfstr (st, xqos->partition.strs[i]);
}

static void print_any_endpoint_common (struct st *st, const struct ddsi_entity_common *e, const struct dds_qos *xqos)
{
  cpfkguid (st, "guid", &e->guid);
  if (xqos->present & DDSI_QP_ENTITY_NAME)
    cpfkstr (st, "name", xqos->entity_name);
  cpfkseq (st, "partitions", print_partition_seq, (struct dds_qos *) xqos);
  if (xqos->present & DDSI_QP_TOPIC_NAME)
    cpfkstr (st, "topic", xqos->topic_name);
  if (xqos->present & DDSI_QP_TYPE_NAME)
    cpfkstr (st, "type", xqos->type_name);
}

static void print_endpoint_common (struct st *st, const struct ddsi_entity_common *e, const struct ddsi_endpoint_common *c, const struct dds_qos *xqos)
{
  DDSRT_UNUSED_ARG (c);
  print_any_endpoint_common (st, e, xqos);
}

static void print_proxy_endpoint_common (struct st *st, const struct ddsi_entity_common *e, const struct ddsi_proxy_endpoint_common *c)
{
  print_any_endpoint_common (st, e, c->xqos);
  cpfkseq (st, "as", print_addrset, c->as);
}

struct print_reader_arg {
  struct ddsi_participant *p;
  struct ddsi_reader *r;
};

#ifdef DDS_HAS_NETWORK_PARTITIONS
static void print_nwpart_seq (struct st *st, void *vr)
{
  struct ddsi_reader * const r = vr;
  char buf[DDSI_LOCSTRLEN];
  for (const struct ddsi_networkpartition_address *a = r->uc_as; a != NULL; a = a->next)
    cpfstr (st, ddsi_locator_to_string (buf, sizeof(buf), &a->loc));
  for (const struct ddsi_networkpartition_address *a = r->mc_as; a != NULL; a = a->next)
    cpfstr (st, ddsi_locator_to_string (buf, sizeof(buf), &a->loc));
}
#endif

static void print_reader_wrseq (struct st *st, void *vr)
{
  struct ddsi_reader * const r = vr;
  ddsrt_avl_iter_t it;
  for (struct ddsi_rd_wr_match *m = ddsrt_avl_iter_first (&ddsi_rd_local_writers_treedef, &r->local_writers, &it); m; m = ddsrt_avl_iter_next (&it))
    cpfguid (st, &m->wr_guid);
}

static void print_reader_pwrseq (struct st *st, void *vr)
{
  struct ddsi_reader * const r = vr;
  ddsrt_avl_iter_t it;
  for (struct ddsi_rd_pwr_match *m = ddsrt_avl_iter_first (&ddsi_rd_writers_treedef, &r->writers, &it); m; m = ddsrt_avl_iter_next (&it))
    cpfguid (st, &m->pwr_guid);
}

static void print_reader (struct st *st, void *varg)
{
  struct print_reader_arg * const arg = varg;
  struct ddsi_reader * const r = arg->r;
  ddsrt_mutex_lock (&r->e.lock);
  print_endpoint_common (st, &r->e, &r->c, r->xqos);
#ifdef DDS_HAS_NETWORK_PARTITIONS
  if (r->uc_as || r->mc_as)
    cpfobj (st, print_nwpart_seq, r);
#endif
  cpfkseq (st, "local_writers", print_reader_wrseq, r);
  cpfkseq (st, "proxy_writers", print_reader_pwrseq, r);
  ddsrt_mutex_unlock (&r->e.lock);
}

struct print_writer_arg {
  struct ddsi_participant *p;
  struct ddsi_writer *w;
};

static void print_whc_state (struct st *st, void *vw)
{
  struct ddsi_writer * const w = vw;
  struct ddsi_whc_state whcst;
  ddsi_whc_get_state (w->whc, &whcst);
  cpfkseqno (st, "min_seq", whcst.min_seq);
  cpfkseqno (st, "max_seq", whcst.max_seq);
  cpfksize (st, "unacked_bytes", whcst.unacked_bytes);
  cpfku32 (st, "whc_low", w->whc_low);
  cpfku32 (st, "whc_high", w->whc_high);
  cpfkseqno (st, "max_drop_seq", ddsi_writer_max_drop_seq (w));
}

static void print_writer_hb (struct st *st, void *vw)
{
  struct ddsi_writer * const w = vw;
  cpfku32 (st, "n_since_last_write", w->hbcontrol.hbs_since_last_write);
  cpfki64 (st, "t_last_nonfinal_hb", w->hbcontrol.t_of_last_ackhb.v);
  cpfki64 (st, "t_last_hb", w->hbcontrol.t_of_last_hb.v);
  cpfki64 (st, "t_last_write", w->hbcontrol.t_of_last_write.v);
  cpfki64 (st, "t_sched", w->hbcontrol.tsched.v);
  cpfku32 (st, "n_reliable_readers", w->num_reliable_readers);
}

static void print_writer_ack (struct st *st, void *vw)
{
  struct ddsi_writer * const w = vw;
  cpfku32 (st, "n_acks_received", w->num_acks_received);
  cpfku32 (st, "n_nacks_received", w->num_nacks_received);
  cpfku32 (st, "rexmit_count", w->rexmit_count);
  cpfku32 (st, "rexmit_lost_count", w->rexmit_lost_count);
  cpfku32 (st, "throttle_count", w->throttle_count);
}

static void print_writer_rdseq (struct st *st, void *vw)
{
  struct ddsi_writer * const w = vw;
  ddsrt_avl_iter_t it;
  for (struct ddsi_wr_rd_match *m = ddsrt_avl_iter_first (&ddsi_wr_local_readers_treedef, &w->local_readers, &it); m; m = ddsrt_avl_iter_next (&it))
    cpfguid (st, &m->rd_guid);
}

static void print_writer_rd (struct st *st, void *vm)
{
  struct ddsi_wr_prd_match * const m = vm;
  cpfkguid (st, "guid", &m->prd_guid);
  cpfkbool (st, "reliable", m->is_reliable);
  cpfkbool (st, "assumed_in_sync", m->assumed_in_sync);
  cpfkbool (st, "has_replied_to_hb", m->has_replied_to_hb);
  cpfkbool (st, "reliable", m->is_reliable);
  cpfkseqno (st, "seq", m->seq);
  cpfku32 (st, "rexmit_requests", m->rexmit_requests);
}

static void print_writer_prdseq (struct st *st, void *vw)
{
  struct ddsi_writer * const w = vw;
  ddsrt_avl_iter_t it;
  for (struct ddsi_wr_prd_match *m = ddsrt_avl_iter_first (&ddsi_wr_readers_treedef, &w->readers, &it); m; m = ddsrt_avl_iter_next (&it))
    cpfobj (st, print_writer_rd, m);
}

static void print_writer (struct st *st, void *varg)
{
  struct print_writer_arg * const arg = varg;
  struct ddsi_writer * const w = arg->w;
  ddsrt_mutex_lock (&w->e.lock);
  print_endpoint_common (st, &w->e, &w->c, w->xqos);
  cpfkobj (st, "whc", print_whc_state, w);
  cpfkseqno (st, "seq", w->seq);
  cpfkseqno (st, "seq_xmit", ddsi_writer_read_seq_xmit (w));
  cpfkbool (st, "throttling", w->throttling);
  cpfkbool (st, "reliable", w->reliable);
  if (w->reliable)
  {
    cpfkobj (st, "heartbeat", print_writer_hb, w);
    cpfkobj (st, "ack", print_writer_ack, w);
  }
  cpfku64 (st, "rexmit_bytes", w->rexmit_bytes);
  cpfku32 (st, "throttle_count", w->throttle_count);
  cpfku64 (st, "time_throttled", w->time_throttled);
  cpfku64 (st, "time_retransmit", w->time_retransmit);

  cpfkseq (st, "as", print_addrset, w->as);
  cpfkseq (st, "local_readers", print_writer_rdseq, w);
  cpfkseq (st, "proxy_readers", print_writer_prdseq, w);
  ddsrt_mutex_unlock (&w->e.lock);
}

struct print_reader_seq_arg {
  struct ddsi_participant *p;
  struct ddsi_entity_enum_reader *er;
};

static void print_reader_seq (struct st *st, void *varg)
{
  struct print_reader_seq_arg * const arg = varg;
  struct ddsi_reader *r;
  while (!st->error && (r = ddsi_entidx_enum_reader_next (arg->er)) != NULL)
    if (r->c.pp == arg->p)
      cpfobj (st, print_reader, &(struct print_reader_arg){ .p = arg->p, .r = r });
}

struct print_writer_seq_arg {
  struct ddsi_participant *p;
  struct ddsi_entity_enum_writer *ew;
};

static void print_writer_seq (struct st *st, void *varg)
{
  struct print_writer_seq_arg * const arg = varg;
  struct ddsi_writer *w;
  while (!st->error && (w = ddsi_entidx_enum_writer_next (arg->ew)) != NULL)
    if (w->c.pp == arg->p)
      cpfobj (st, print_writer, &(struct print_writer_arg){ .p = arg->p, .w = w });
}

static void print_participant_flags (struct st *st, void *vp)
{
  struct ddsi_participant * const p = vp;
  if (p->is_ddsi2_pp)
    cpfstr (st, "ddsi2");
}

static void print_participant (struct st *st, void *vp)
{
  struct ddsi_participant *p = vp;

  ddsrt_mutex_lock (&p->e.lock);
  cpfkguid (st, "guid", &p->e.guid);
  cpfkstr (st, "name", (p->plist->qos.present & DDSI_QP_ENTITY_NAME) ? p->plist->qos.entity_name : "");
  cpfkseq (st, "flags", print_participant_flags, p);
  ddsrt_mutex_unlock (&p->e.lock);

  {
    struct ddsi_entity_enum_reader er;
    ddsi_entidx_enum_reader_init (&er, st->gv->entity_index);
    cpfkseq (st, "readers", print_reader_seq, &(struct print_reader_seq_arg){ .p = p, .er = &er });
    ddsi_entidx_enum_reader_fini (&er);
  }

  {
    struct ddsi_entity_enum_writer ew;
    ddsi_entidx_enum_writer_init (&ew, st->gv->entity_index);
    cpfkseq (st, "writers", print_writer_seq, &(struct print_writer_seq_arg){ .p = p, .ew = &ew });
    ddsi_entidx_enum_writer_fini (&ew);
  }
}

static void print_participants_seq (struct st *st, void *ve)
{
  struct ddsi_entity_enum_participant *e = ve;
  struct ddsi_participant *p;
  while (!st->error && (p = ddsi_entidx_enum_participant_next (e)) != NULL)
    cpfobj (st, print_participant, p);
}

static void print_participants (struct st *st)
{
  struct ddsi_entity_enum_participant e;
  ddsi_thread_state_awake_fixed_domain (st->thrst);
  ddsi_entidx_enum_participant_init (&e, st->gv->entity_index);
  cpfkseq (st, "participants", print_participants_seq, &e);
  ddsi_entidx_enum_participant_fini (&e);
  ddsi_thread_state_asleep (st->thrst);
}

struct print_proxy_reader_arg {
  struct ddsi_proxy_participant *p;
  struct ddsi_proxy_reader *r;
};

static void print_proxy_reader_wrseq (struct st *st, void *vr)
{
  struct ddsi_proxy_reader * const r = vr;
  ddsrt_avl_iter_t it;
  for (struct ddsi_prd_wr_match *m = ddsrt_avl_iter_first (&ddsi_prd_writers_treedef, &r->writers, &it); m; m = ddsrt_avl_iter_next (&it))
    cpfguid (st, &m->wr_guid);
}

static void print_proxy_reader (struct st *st, void *varg)
{
  struct print_proxy_reader_arg * const arg = varg;
  struct ddsi_proxy_reader * const r = arg->r;
  ddsrt_mutex_lock (&r->e.lock);
  print_proxy_endpoint_common (st, &r->e, &r->c);
  cpfkseq (st, "local_writers", print_proxy_reader_wrseq, r);
  ddsrt_mutex_unlock (&r->e.lock);
}

struct print_proxy_reader_seq_arg {
  struct ddsi_proxy_participant *p;
  struct ddsi_entity_enum_proxy_reader *er;
};

static void print_proxy_reader_seq (struct st *st, void *varg)
{
  struct print_proxy_reader_seq_arg * const arg = varg;
  struct ddsi_proxy_reader *r;
  while (!st->error && (r = ddsi_entidx_enum_proxy_reader_next (arg->er)) != NULL)
    if (r->c.proxypp == arg->p)
      cpfobj (st, print_proxy_reader, &(struct print_proxy_reader_arg){ .p = arg->p, .r = r });
}

static void print_proxy_writer_rd (struct st *st, void *vm)
{
  struct ddsi_pwr_rd_match * const m = vm;
  cpfkguid (st, "guid", &m->rd_guid);
  cpfkseqno (st, "last_nack_seq_end_p1", m->last_nack.seq_end_p1);
  cpfku32 (st, "last_nack_frag_end_p1", m->last_nack.frag_end_p1);
  cpfki64 (st, "t_last_nack", m->t_last_nack.v);
  switch (m->in_sync)
  {
    case PRMSS_SYNC:
      cpfkstr (st, "in_sync", "sync");
      break;
    case PRMSS_TLCATCHUP:
      cpfkstr (st, "in_sync", "tlcatchup");
      cpfkseqno (st, "end_of_tl_seq", m->u.not_in_sync.end_of_tl_seq);
      break;
    case PRMSS_OUT_OF_SYNC:
      cpfkstr (st, "in_sync", "out_of_sync");
      cpfkseqno (st, "end_of_tl_seq", m->u.not_in_sync.end_of_tl_seq);
      break;
  }
}

static void print_proxy_writer_rdseq (struct st *st, void *vw)
{
  struct ddsi_proxy_writer * const w = vw;
  ddsrt_avl_iter_t it;
  for (struct ddsi_pwr_rd_match *m = ddsrt_avl_iter_first (&ddsi_pwr_readers_treedef, &w->readers, &it); m; m = ddsrt_avl_iter_next (&it))
    cpfobj (st, print_proxy_writer_rd, m);
}

struct print_proxy_writer_arg {
  struct ddsi_proxy_participant *p;
  struct ddsi_proxy_writer *w;
};

static void print_proxy_writer (struct st *st, void *varg)
{
  struct print_proxy_writer_arg * const arg = varg;
  struct ddsi_proxy_writer * const w = arg->w;
  ddsrt_mutex_lock (&w->e.lock);
  print_proxy_endpoint_common (st, &w->e, &w->c);
  cpfkseqno (st, "last_seq", w->last_seq);
  cpfku32 (st, "last_fragnum", w->last_fragnum);
  cpfkseq (st, "local_readers", print_proxy_writer_rdseq, w);
  uint64_t disc_frags, disc_samples;
  ddsi_defrag_stats (w->defrag, &disc_frags);
  ddsi_reorder_stats (w->reorder, &disc_samples);
  cpfku64 (st, "discarded_fragment_bytes", disc_frags);
  cpfku64 (st, "discarded_sample_bytes", disc_samples);
  ddsrt_mutex_unlock (&w->e.lock);
}

struct print_proxy_writer_seq_arg {
  struct ddsi_proxy_participant *p;
  struct ddsi_entity_enum_proxy_writer *ew;
};

static void print_proxy_writer_seq (struct st *st, void *varg)
{
  struct print_proxy_writer_seq_arg * const arg = varg;
  struct ddsi_proxy_writer *w;
  while (!st->error && (w = ddsi_entidx_enum_proxy_writer_next (arg->ew)) != NULL)
    if (w->c.proxypp == arg->p)
      cpfobj (st, print_proxy_writer, &(struct print_proxy_writer_arg){ .p = arg->p, .w = w });
}

static void print_proxy_participant_flags (struct st *st, void *vp)
{
  struct ddsi_proxy_participant * const p = vp;
  if (p->implicitly_created)
    cpfstr (st, "implicitly_created");
  if (p->is_ddsi2_pp)
    cpfstr (st, "ddsi2");
  if (p->minimal_bes_mode)
    cpfstr (st, "minimal_bes_mode");
  if (p->redundant_networking)
    cpfstr (st, "redundant_networking");
}

static void print_proxy_participant (struct st *st, void *vp)
{
  struct ddsi_proxy_participant * const p = vp;

  ddsrt_mutex_lock (&p->e.lock);
  cpfkguid (st, "guid", &p->e.guid);
  cpfkseq (st, "flags", print_proxy_participant_flags, p);
  ddsrt_mutex_unlock (&p->e.lock);
  cpfkseq (st, "as_data", print_addrset, p->as_default);
  cpfkseq (st, "as_meta", print_addrset, p->as_meta);

  {
    struct ddsi_entity_enum_proxy_reader er;
    ddsi_entidx_enum_proxy_reader_init (&er, st->gv->entity_index);
    cpfkseq (st, "proxy_readers", print_proxy_reader_seq, &(struct print_proxy_reader_seq_arg){ .p = p, .er = &er });
    ddsi_entidx_enum_proxy_reader_fini (&er);
  }

  {
    struct ddsi_entity_enum_proxy_writer ew;
    ddsi_entidx_enum_proxy_writer_init (&ew, st->gv->entity_index);
    cpfkseq (st, "proxy_writers", print_proxy_writer_seq, &(struct print_proxy_writer_seq_arg){ .p = p, .ew = &ew });
    ddsi_entidx_enum_proxy_writer_fini (&ew);
  }
}

static void print_proxy_participants_seq (struct st *st, void *ve)
{
  struct ddsi_entity_enum_proxy_participant *e = ve;
  struct ddsi_proxy_participant *p;
  while (!st->error && (p = ddsi_entidx_enum_proxy_participant_next (e)) != NULL)
    cpfobj (st, print_proxy_participant, p);
}

static void print_proxy_participants (struct st *st)
{
  struct ddsi_entity_enum_proxy_participant e;
  ddsi_thread_state_awake_fixed_domain (st->thrst);
  ddsi_entidx_enum_proxy_participant_init (&e, st->gv->entity_index);
  cpfkseq (st, "proxy_participants", print_proxy_participants_seq, &e);
  ddsi_entidx_enum_proxy_participant_fini (&e);
  ddsi_thread_state_asleep (st->thrst);
}

static void print_domain (struct st *st, void *varg)
{
  (void) varg;
  print_participants (st);
  print_proxy_participants (st);
}

static void debmon_handle_connection (struct ddsi_debug_monitor *dm, struct ddsi_tran_conn * conn)
{
  ddsi_locator_t loc;
  const char *http_header = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n";
  const ddsrt_iovec_t iov = { .iov_base = (void *) http_header, .iov_len = (ddsrt_iov_len_t) strlen (http_header) };

  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  struct st st = {
    .conn = conn,
    .gv = dm->gv,
    .thrst = thrst,
    .error = false,
    .comma = "",
    .pos = 8
  };

  if (!ddsi_conn_peer_locator(st.conn, &loc)) {
    return;
  }

  if (ddsi_conn_write (st.conn, &loc, 1, &iov, 0) < 0) {
    // If we cant even send headers dont bother with encoding the rest
    return;
  }

  // Encode data
  cpfobj (&st, print_domain, NULL);

  // Last content chunk
  if (st.pos > 8)
    cpemitchunk(&st, loc);

  // Terminating chunk
  cpemitchunk(&st, loc);
}

static uint32_t debmon_main (void *vdm)
{
  struct ddsi_debug_monitor *dm = vdm;
  ddsrt_mutex_lock (&dm->lock);
  while (!dm->stop)
  {
    ddsrt_mutex_unlock (&dm->lock);
    struct ddsi_tran_conn * conn = ddsi_listener_accept (dm->servsock);
    ddsrt_mutex_lock (&dm->lock);
    if (conn != NULL && !dm->stop)
    {
      ddsrt_mutex_unlock (&dm->lock);
      debmon_handle_connection (dm, conn);
      ddsrt_mutex_lock (&dm->lock);
    }
    if (conn != NULL)
    {
      ddsi_conn_free (conn);
    }
  }
  ddsrt_mutex_unlock (&dm->lock);
  return 0;
}

struct ddsi_debug_monitor *ddsi_new_debug_monitor (struct ddsi_domaingv *gv, int32_t port)
{
  struct ddsi_debug_monitor *dm;

  /* negative port number means the feature is disabled */
  if (gv->config.monitor_port < 0)
    return NULL;

  if (ddsi_tcp_init (gv) < 0)
    return NULL;

  dm = ddsrt_malloc (sizeof (*dm));

  dm->gv = gv;
  if ((dm->tran_factory = ddsi_factory_find (gv, "tcp")) == NULL)
    dm->tran_factory = ddsi_factory_find (gv, "tcp6");

  if (!ddsi_is_valid_port (dm->tran_factory, (uint32_t) port))
  {
    GVERROR ("debug monitor port number %"PRId32" is invalid\n", port);
    goto err_invalid_port;
  }

  if (ddsi_factory_create_listener (&dm->servsock, dm->tran_factory, (uint32_t) port, NULL) != DDS_RETCODE_OK)
  {
    GVWARNING ("debmon: can't create socket\n");
    goto err_servsock;
  }

  {
    char buf[DDSI_LOCSTRLEN];
    (void) ddsi_listener_locator(dm->servsock, &dm->servlocator);
    GVLOG (DDS_LC_CONFIG, "debmon at %s\n", ddsi_locator_to_string (buf, sizeof(buf), &dm->servlocator));
  }

  ddsrt_mutex_init (&dm->lock);
  ddsrt_cond_init (&dm->cond);
  if (ddsi_listener_listen (dm->servsock) < 0)
    goto err_listen;
  dm->stop = 0;
  if (ddsi_create_thread (&dm->servts, gv, "debmon", debmon_main, dm) != DDS_RETCODE_OK)
    goto err_listen;
  return dm;

err_listen:
  ddsrt_cond_destroy(&dm->cond);
  ddsrt_mutex_destroy(&dm->lock);
  ddsi_listener_free(dm->servsock);
err_servsock:
err_invalid_port:
  ddsrt_free(dm);
  return NULL;
}

bool ddsi_get_debug_monitor_locator (struct ddsi_debug_monitor *dm, ddsi_locator_t *locator) {
  if (!dm || dm->stop) return false;
  memcpy(locator, &dm->servlocator, sizeof(ddsi_locator_t));
  return true;
}

void ddsi_free_debug_monitor (struct ddsi_debug_monitor *dm)
{
  if (dm == NULL)
    return;

  ddsrt_mutex_lock (&dm->lock);
  dm->stop = 1;
  ddsrt_cond_broadcast (&dm->cond);
  ddsrt_mutex_unlock (&dm->lock);
  ddsi_listener_unblock (dm->servsock);
  ddsi_join_thread (dm->servts);
  ddsi_listener_free (dm->servsock);
  ddsrt_cond_destroy (&dm->cond);
  ddsrt_mutex_destroy (&dm->lock);
  ddsrt_free (dm);
}

