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
#include <stddef.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/misc.h"

#include "dds/ddsrt/avl.h"

#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/q_ddsi_discovery.h"
#include "dds/ddsi/q_protocol.h" /* NN_ENTITYID_... */
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_debmon.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_tran.h"
#include "dds/ddsi/ddsi_tcp.h"

#include "dds__whc.h"

struct plugin {
  debug_monitor_plugin_t fn;
  void *arg;
  struct plugin *next;
};

struct debug_monitor {
  struct thread_state1 *servts;
  ddsi_tran_factory_t tran_factory;
  ddsi_tran_listener_t servsock;
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  struct ddsi_domaingv *gv;
  struct plugin *plugins;
  int stop;
};

static int cpf (ddsi_tran_conn_t conn, const char *fmt, ...) ddsrt_attribute_format_printf(2, 3);

static int cpf (ddsi_tran_conn_t conn, const char *fmt, ...)
{
  ddsi_locator_t loc;
  if (!ddsi_conn_peer_locator (conn, &loc))
    return 0;
  else
  {
    va_list ap;
    ddsrt_iovec_t iov;
    char buf[4096];
    int n;
    va_start (ap, fmt);
    n = vsnprintf (buf, sizeof (buf), fmt, ap);
    va_end (ap);
    iov.iov_base = buf;
    iov.iov_len = (ddsrt_iov_len_t) n;
    return ddsi_conn_write (conn, &loc, 1, &iov, 0) < 0 ? -1 : 0;
  }
}

struct print_address_arg {
  ddsi_tran_conn_t conn;
  int count;
};

static void print_address (const ddsi_xlocator_t *n, void *varg)
{
  struct print_address_arg *arg = varg;
  char buf[DDSI_LOCSTRLEN];
  arg->count += cpf (arg->conn, " %s", ddsi_locator_to_string (buf, sizeof(buf), &n->c));
}

static int print_addrset (ddsi_tran_conn_t conn, const char *prefix, struct addrset *as, const char *suffix)
{
  struct print_address_arg pa_arg;
  pa_arg.conn = conn;
  pa_arg.count = cpf (conn, "%s", prefix);
  addrset_forall(as, print_address, &pa_arg);
  pa_arg.count += cpf (conn, "%s", suffix);
  return pa_arg.count;
}

static int print_addrset_if_notempty (ddsi_tran_conn_t conn, const char *prefix, struct addrset *as, const char *suffix)
{
  if (addrset_empty(as))
    return 0;
  else
    return print_addrset (conn, prefix, as, suffix);
}

static int print_any_endpoint_common (ddsi_tran_conn_t conn, const char *label, const struct entity_common *e, const struct dds_qos *xqos)
{
  int x = 0;
  x += cpf (conn, "  %s "PGUIDFMT" ", label, PGUID (e->guid));
  if (xqos->present & QP_PARTITION)
  {
    if (xqos->partition.n > 1) cpf (conn, "{");
    for (uint32_t i = 0; i < xqos->partition.n; i++)
      x += cpf (conn, "%s%s", i == 0 ? "" : ",", xqos->partition.strs[i]);
    if (xqos->partition.n > 1) cpf (conn, "}");
    const char *topic_name = (xqos->present & QP_TOPIC_NAME) ? xqos->topic_name : "null";
    const char *topic_typename = (xqos->present & QP_TYPE_NAME) ? xqos->type_name : "null";
    x += cpf (conn, ".%s/%s", topic_name, topic_typename);
  }
  cpf (conn, "\n");
  return x;
}

static int print_endpoint_common (ddsi_tran_conn_t conn, const char *label, const struct entity_common *e, const struct endpoint_common *c, const struct dds_qos *xqos)
{
  DDSRT_UNUSED_ARG (c);
  return print_any_endpoint_common (conn, label, e, xqos);
}

static int print_proxy_endpoint_common (ddsi_tran_conn_t conn, const char *label, const struct entity_common *e, const struct proxy_endpoint_common *c)
{
  int x = 0;
  x += print_any_endpoint_common (conn, label, e, c->xqos);
  x += print_addrset_if_notempty (conn, "    as", c->as, "\n");
  return x;
}


static int print_participants (struct thread_state1 * const ts1, struct ddsi_domaingv *gv, ddsi_tran_conn_t conn)
{
  struct entidx_enum_participant e;
  struct participant *p;
  int x = 0;
  thread_state_awake_fixed_domain (ts1);
  entidx_enum_participant_init (&e, gv->entity_index);
  while ((p = entidx_enum_participant_next (&e)) != NULL)
  {
    ddsrt_mutex_lock (&p->e.lock);
    x += cpf (conn, "pp "PGUIDFMT" %s%s\n", PGUID (p->e.guid), p->e.name, p->is_ddsi2_pp ? " [ddsi2]" : "");
    ddsrt_mutex_unlock (&p->e.lock);

    {
      struct entidx_enum_reader er;
      struct reader *r;
      entidx_enum_reader_init (&er, gv->entity_index);
      while ((r = entidx_enum_reader_next (&er)) != NULL)
      {
        ddsrt_avl_iter_t writ;
        struct rd_pwr_match *m;
        if (r->c.pp != p)
          continue;
        ddsrt_mutex_lock (&r->e.lock);
        print_endpoint_common (conn, "rd", &r->e, &r->c, r->xqos);
#ifdef DDS_HAS_NETWORK_PARTITIONS
        if (r->uc_as || r->mc_as)
        {
          char buf[DDSI_LOCSTRLEN];
          x += cpf (conn, "    as");
          for (const struct networkpartition_address *a = r->uc_as; a != NULL; a = a->next)
            x += cpf (conn, " %s", ddsi_locator_to_string (buf, sizeof(buf), &a->loc));
          for (const struct networkpartition_address *a = r->mc_as; a != NULL; a = a->next)
            x += cpf (conn, " %s", ddsi_locator_to_string (buf, sizeof(buf), &a->loc));
          x += cpf (conn, "\n");
        }
#endif
        for (m = ddsrt_avl_iter_first (&rd_writers_treedef, &r->writers, &writ); m; m = ddsrt_avl_iter_next (&writ))
          x += cpf (conn, "    pwr "PGUIDFMT"\n", PGUID (m->pwr_guid));
        ddsrt_mutex_unlock (&r->e.lock);
      }
      entidx_enum_reader_fini (&er);
    }

    {
      struct entidx_enum_writer ew;
      struct writer *w;
      entidx_enum_writer_init (&ew, gv->entity_index);
      while ((w = entidx_enum_writer_next (&ew)) != NULL)
      {
        ddsrt_avl_iter_t rdit;
        struct wr_prd_match *m;
        struct whc_state whcst;
        if (w->c.pp != p)
          continue;
        ddsrt_mutex_lock (&w->e.lock);
        print_endpoint_common (conn, "wr", &w->e, &w->c, w->xqos);
        whc_get_state(w->whc, &whcst);
        x += cpf (conn, "    whc [%"PRIu64",%"PRIu64"] unacked %"PRIuSIZE"%s [%"PRIu32",%"PRIu32"] seq %"PRIu64" seq_xmit %"PRIu64" cs_seq %"PRIu64"\n",
                  whcst.min_seq, whcst.max_seq, whcst.unacked_bytes,
                  w->throttling ? " THROTTLING" : "",
                  w->whc_low, w->whc_high,
                  w->seq, writer_read_seq_xmit (w), w->cs_seq);
        if (w->reliable)
        {
          x += cpf (conn, "    hb %"PRIu32" ackhb %"PRId64" hb %"PRId64" wr %"PRId64" sched %"PRId64" #rel %"PRId32"\n",
                    w->hbcontrol.hbs_since_last_write, w->hbcontrol.t_of_last_ackhb.v,
                    w->hbcontrol.t_of_last_hb.v, w->hbcontrol.t_of_last_write.v,
                    w->hbcontrol.tsched.v, w->num_reliable_readers);
          x += cpf (conn, "    #acks %"PRIu32" #nacks %"PRIu32" #rexmit %"PRIu32" #lost %"PRIu32" #throttle %"PRIu32"\n",
                    w->num_acks_received, w->num_nacks_received, w->rexmit_count, w->rexmit_lost_count, w->throttle_count);
          x += cpf (conn, "    max-drop-seq %"PRIu64"\n", writer_max_drop_seq (w));
        }
        x += print_addrset_if_notempty (conn, "    as", w->as, "\n");
        for (m = ddsrt_avl_iter_first (&wr_readers_treedef, &w->readers, &rdit); m; m = ddsrt_avl_iter_next (&rdit))
        {
          char wr_prd_flags[4];
          wr_prd_flags[0] = m->is_reliable ? 'R' : 'U';
          wr_prd_flags[1] = m->assumed_in_sync ? 's' : '.';
          wr_prd_flags[2] = m->has_replied_to_hb ? 'a' : '.'; /* a = ack seen */
          wr_prd_flags[3] = 0;
          x += cpf (conn, "    prd "PGUIDFMT" %s @ %"PRIu64" [%"PRIu64",%"PRIu64"] #nacks %"PRIu32"\n",
                    PGUID (m->prd_guid), wr_prd_flags, m->seq, m->min_seq, m->max_seq, m->rexmit_requests);
        }
        ddsrt_mutex_unlock (&w->e.lock);
      }
      entidx_enum_writer_fini (&ew);
    }
  }
  entidx_enum_participant_fini (&e);
  thread_state_asleep (ts1);
  return x;
}

static int print_proxy_participants (struct thread_state1 * const ts1, struct ddsi_domaingv *gv, ddsi_tran_conn_t conn)
{
  struct entidx_enum_proxy_participant e;
  struct proxy_participant *p;
  int x = 0;
  thread_state_awake_fixed_domain (ts1);
  entidx_enum_proxy_participant_init (&e, gv->entity_index);
  while ((p = entidx_enum_proxy_participant_next (&e)) != NULL)
  {
    ddsrt_mutex_lock (&p->e.lock);
    x += cpf (conn, "proxypp "PGUIDFMT"%s\n", PGUID (p->e.guid), p->is_ddsi2_pp ? " [ddsi2]" : "");
    ddsrt_mutex_unlock (&p->e.lock);
    x += print_addrset (conn, "  as data", p->as_default, "");
    x += print_addrset (conn, " meta", p->as_default, "\n");

    {
      struct entidx_enum_proxy_reader er;
      struct proxy_reader *r;
      entidx_enum_proxy_reader_init (&er, gv->entity_index);
      while ((r = entidx_enum_proxy_reader_next (&er)) != NULL)
      {
        ddsrt_avl_iter_t writ;
        struct prd_wr_match *m;
        if (r->c.proxypp != p)
          continue;
        ddsrt_mutex_lock (&r->e.lock);
        print_proxy_endpoint_common (conn, "prd", &r->e, &r->c);
        for (m = ddsrt_avl_iter_first (&rd_writers_treedef, &r->writers, &writ); m; m = ddsrt_avl_iter_next (&writ))
          x += cpf (conn, "    wr "PGUIDFMT"\n", PGUID (m->wr_guid));
        ddsrt_mutex_unlock (&r->e.lock);
      }
      entidx_enum_proxy_reader_fini (&er);
    }

    {
      struct entidx_enum_proxy_writer ew;
      struct proxy_writer *w;
      entidx_enum_proxy_writer_init (&ew, gv->entity_index);
      while ((w = entidx_enum_proxy_writer_next (&ew)) != NULL)
      {
        ddsrt_avl_iter_t rdit;
        struct pwr_rd_match *m;
        if (w->c.proxypp != p)
          continue;
        ddsrt_mutex_lock (&w->e.lock);
        print_proxy_endpoint_common (conn, "pwr", &w->e, &w->c);
        x += cpf (conn, "    last_seq %"PRIu64" last_fragnum %"PRIu32"\n", w->last_seq, w->last_fragnum);
        for (m = ddsrt_avl_iter_first (&wr_readers_treedef, &w->readers, &rdit); m; m = ddsrt_avl_iter_next (&rdit))
        {
          x += cpf (conn, "    rd "PGUIDFMT" (nack %"PRIu64" frag %"PRIu32" %"PRId64")\n",
                    PGUID (m->rd_guid), m->last_nack.seq_end_p1, m->last_nack.frag_end_p1, m->t_last_nack.v);
          switch (m->in_sync)
          {
            case PRMSS_SYNC:
              break;
            case PRMSS_TLCATCHUP:
              x += cpf (conn, "      tl-catchup end_of_tl_seq %"PRIu64"\n", m->u.not_in_sync.end_of_tl_seq);
              break;
            case PRMSS_OUT_OF_SYNC:
              x += cpf (conn, "      out-of-sync end_of_tl_seq %"PRIu64"\n", m->u.not_in_sync.end_of_tl_seq);
              break;
          }
        }
        ddsrt_mutex_unlock (&w->e.lock);
      }
      entidx_enum_proxy_writer_fini (&ew);
    }
  }
  entidx_enum_proxy_participant_fini (&e);
  thread_state_asleep (ts1);
  return x;
}

static void debmon_handle_connection (struct debug_monitor *dm, ddsi_tran_conn_t conn)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  struct plugin *p;
  int r = 0;
  r += print_participants (ts1, dm->gv, conn);
  if (r == 0)
    r += print_proxy_participants (ts1, dm->gv, conn);

  /* Note: can only add plugins (at the tail) */
  ddsrt_mutex_lock (&dm->lock);
  p = dm->plugins;
  while (r == 0 && p != NULL)
  {
    ddsrt_mutex_unlock (&dm->lock);
    r += p->fn (conn, cpf, p->arg);
    ddsrt_mutex_lock (&dm->lock);
    p = p->next;
  }
  ddsrt_mutex_unlock (&dm->lock);
}

static uint32_t debmon_main (void *vdm)
{
  struct debug_monitor *dm = vdm;
  ddsrt_mutex_lock (&dm->lock);
  while (!dm->stop)
  {
    ddsrt_mutex_unlock (&dm->lock);
    ddsi_tran_conn_t conn = ddsi_listener_accept (dm->servsock);
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

struct debug_monitor *new_debug_monitor (struct ddsi_domaingv *gv, int32_t port)
{
  struct debug_monitor *dm;

  /* negative port number means the feature is disabled */
  if (gv->config.monitor_port < 0)
    return NULL;

  if (ddsi_tcp_init (gv) < 0)
    return NULL;

  dm = ddsrt_malloc (sizeof (*dm));

  dm->gv = gv;
  dm->plugins = NULL;
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
    ddsi_locator_t loc;
    char buf[DDSI_LOCSTRLEN];
    (void) ddsi_listener_locator(dm->servsock, &loc);
    GVLOG (DDS_LC_CONFIG, "debmon at %s\n", ddsi_locator_to_string (buf, sizeof(buf), &loc));
  }

  ddsrt_mutex_init (&dm->lock);
  ddsrt_cond_init (&dm->cond);
  if (ddsi_listener_listen (dm->servsock) < 0)
    goto err_listen;
  dm->stop = 0;
  if (create_thread (&dm->servts, gv, "debmon", debmon_main, dm) != DDS_RETCODE_OK)
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

void add_debug_monitor_plugin (struct debug_monitor *dm, debug_monitor_plugin_t fn, void *arg)
{
  struct plugin *p, **pp;
  if (dm != NULL && (p = ddsrt_malloc (sizeof (*p))) != NULL)
  {
    p->fn = fn;
    p->arg = arg;
    p->next = NULL;
    ddsrt_mutex_lock (&dm->lock);
    pp = &dm->plugins;
    while (*pp)
      pp = &(*pp)->next;
    *pp = p;
    ddsrt_mutex_unlock (&dm->lock);
  }
}

void free_debug_monitor (struct debug_monitor *dm)
{
  if (dm == NULL)
    return;

  ddsrt_mutex_lock (&dm->lock);
  dm->stop = 1;
  ddsrt_cond_broadcast (&dm->cond);
  ddsrt_mutex_unlock (&dm->lock);
  ddsi_listener_unblock (dm->servsock);
  join_thread (dm->servts);
  ddsi_listener_free (dm->servsock);
  ddsrt_cond_destroy (&dm->cond);
  ddsrt_mutex_destroy (&dm->lock);

  while (dm->plugins) {
    struct plugin *p = dm->plugins;
    dm->plugins = p->next;
    ddsrt_free (p);
  }
  ddsrt_free (dm);
}

