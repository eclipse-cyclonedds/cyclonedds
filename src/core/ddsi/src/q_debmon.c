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

#include "os/os.h"

#include "util/ut_avl.h"

#include "ddsi/q_entity.h"
#include "ddsi/q_config.h"
#include "ddsi/q_time.h"
#include "ddsi/q_misc.h"
#include "ddsi/q_log.h"
#include "ddsi/q_plist.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_globals.h"
#include "ddsi/q_addrset.h"
#include "ddsi/q_radmin.h"
#include "ddsi/q_ddsi_discovery.h"
#include "ddsi/q_protocol.h" /* NN_ENTITYID_... */
#include "ddsi/q_unused.h"
#include "ddsi/q_error.h"
#include "ddsi/q_debmon.h"
#include "ddsi/ddsi_serdata.h"
#include "ddsi/ddsi_tran.h"
#include "ddsi/ddsi_tcp.h"

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
  os_mutex lock;
  os_cond cond;
  struct plugin *plugins;
  int stop;
};

static int cpf (ddsi_tran_conn_t conn, const char *fmt, ...)
{
  nn_locator_t loc;
  if (!ddsi_conn_peer_locator (conn, &loc))
    return 0;
  else
  {
    va_list ap;
    os_iovec_t iov;
    char buf[4096];
    int n;
    va_start (ap, fmt);
    n = os_vsnprintf (buf, sizeof (buf), fmt, ap);
    va_end (ap);
    iov.iov_base = buf;
    iov.iov_len = (size_t) n;
    return ddsi_conn_write (conn, &loc, 1, &iov, 0) < 0 ? -1 : 0;
  }
}

struct print_address_arg {
  ddsi_tran_conn_t conn;
  int count;
};

static void print_address (const nn_locator_t *n, void *varg)
{
  struct print_address_arg *arg = varg;
  char buf[DDSI_LOCSTRLEN];
  arg->count += cpf (arg->conn, " %s", ddsi_locator_to_string (buf, sizeof(buf), n));
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

static int print_any_endpoint_common (ddsi_tran_conn_t conn, const char *label, const struct entity_common *e,
                                      const struct nn_xqos *xqos, const struct ddsi_sertopic *topic)
{
  int x = 0;
  x += cpf (conn, "  %s %x:%x:%x:%x ", label, PGUID (e->guid));
  if (xqos->present & QP_PARTITION)
  {
    unsigned i;
    if (xqos->partition.n > 1) cpf (conn, "{");
    for (i = 0; i < xqos->partition.n; i++)
      x += cpf (conn, "%s%s", i == 0 ? "" : ",", xqos->partition.strs[i]);
    if (xqos->partition.n > 1) cpf (conn, "}");
    x += cpf (conn, ".%s/%s",
              topic && topic->name ? topic->name : (xqos->present & QP_TOPIC_NAME) ? xqos->topic_name : "(null)",
              topic && topic->typename ? topic->typename : (xqos->present & QP_TYPE_NAME) ? xqos->type_name : "(null)");
  }
  cpf (conn, "\n");
  return x;
}

static int print_endpoint_common (ddsi_tran_conn_t conn, const char *label, const struct entity_common *e, const struct endpoint_common *c, const struct nn_xqos *xqos, const struct ddsi_sertopic *topic)
{
  OS_UNUSED_ARG (c);
  return print_any_endpoint_common (conn, label, e, xqos, topic);
}

static int print_proxy_endpoint_common (ddsi_tran_conn_t conn, const char *label, const struct entity_common *e, const struct proxy_endpoint_common *c)
{
  int x = 0;
  x += print_any_endpoint_common (conn, label, e, c->xqos, c->topic);
  x += print_addrset_if_notempty (conn, "    as", c->as, "\n");
  return x;
}


static int print_participants (struct thread_state1 *self, ddsi_tran_conn_t conn)
{
  struct ephash_enum_participant e;
  struct participant *p;
  int x = 0;
  thread_state_awake (self);
  ephash_enum_participant_init (&e);
  while ((p = ephash_enum_participant_next (&e)) != NULL)
  {
    os_mutexLock (&p->e.lock);
    x += cpf (conn, "pp %x:%x:%x:%x %s%s\n", PGUID (p->e.guid), p->e.name, p->is_ddsi2_pp ? " [ddsi2]" : "");
    os_mutexUnlock (&p->e.lock);

    {
      struct ephash_enum_reader er;
      struct reader *r;
      ephash_enum_reader_init (&er);
      while ((r = ephash_enum_reader_next (&er)) != NULL)
      {
        ut_avlIter_t writ;
        struct rd_pwr_match *m;
        if (r->c.pp != p)
          continue;
        os_mutexLock (&r->e.lock);
        print_endpoint_common (conn, "rd", &r->e, &r->c, r->xqos, r->topic);
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
        x += print_addrset_if_notempty (conn, "    as", r->as, "\n");
#endif
        for (m = ut_avlIterFirst (&rd_writers_treedef, &r->writers, &writ); m; m = ut_avlIterNext (&writ))
          x += cpf (conn, "    pwr %x:%x:%x:%x\n", PGUID (m->pwr_guid));
        os_mutexUnlock (&r->e.lock);
      }
      ephash_enum_reader_fini (&er);
    }

    {
      struct ephash_enum_writer ew;
      struct writer *w;
      ephash_enum_writer_init (&ew);
      while ((w = ephash_enum_writer_next (&ew)) != NULL)
      {
        ut_avlIter_t rdit;
        struct wr_prd_match *m;
        struct whc_state whcst;
        if (w->c.pp != p)
          continue;
        os_mutexLock (&w->e.lock);
        print_endpoint_common (conn, "wr", &w->e, &w->c, w->xqos, w->topic);
        whc_get_state(w->whc, &whcst);
        x += cpf (conn, "    whc [%lld,%lld] unacked %"PRIuSIZE"%s [%u,%u] seq %lld seq_xmit %lld cs_seq %lld\n",
                  whcst.min_seq, whcst.max_seq, whcst.unacked_bytes,
                  w->throttling ? " THROTTLING" : "",
                  w->whc_low, w->whc_high,
                  w->seq, READ_SEQ_XMIT(w), w->cs_seq);
        if (w->reliable)
        {
          x += cpf (conn, "    hb %u ackhb %lld hb %lld wr %lld sched %lld #rel %d\n",
                    w->hbcontrol.hbs_since_last_write, w->hbcontrol.t_of_last_ackhb,
                    w->hbcontrol.t_of_last_hb, w->hbcontrol.t_of_last_write,
                    w->hbcontrol.tsched, w->num_reliable_readers);
          x += cpf (conn, "    #acks %u #nacks %u #rexmit %u #lost %u #throttle %u\n",
                    w->num_acks_received, w->num_nacks_received, w->rexmit_count, w->rexmit_lost_count, w->throttle_count);
          x += cpf (conn, "    max-drop-seq %lld\n", writer_max_drop_seq (w));
        }
        x += print_addrset_if_notempty (conn, "    as", w->as, "\n");
        for (m = ut_avlIterFirst (&wr_readers_treedef, &w->readers, &rdit); m; m = ut_avlIterNext (&rdit))
        {
          char wr_prd_flags[4];
          wr_prd_flags[0] = m->is_reliable ? 'R' : 'U';
          wr_prd_flags[1] = m->assumed_in_sync ? 's' : '.';
          wr_prd_flags[2] = m->has_replied_to_hb ? 'a' : '.'; /* a = ack seen */
          wr_prd_flags[3] = 0;
          x += cpf (conn, "    prd %x:%x:%x:%x %s @ %lld [%lld,%lld] #nacks %u\n",
                    PGUID (m->prd_guid), wr_prd_flags, m->seq, m->min_seq, m->max_seq, m->rexmit_requests);
        }
        os_mutexUnlock (&w->e.lock);
      }
      ephash_enum_writer_fini (&ew);
    }
  }
  ephash_enum_participant_fini (&e);
  thread_state_asleep (self);
  return x;
}

static int print_proxy_participants (struct thread_state1 *self, ddsi_tran_conn_t conn)
{
  struct ephash_enum_proxy_participant e;
  struct proxy_participant *p;
  int x = 0;
  thread_state_awake (self);
  ephash_enum_proxy_participant_init (&e);
  while ((p = ephash_enum_proxy_participant_next (&e)) != NULL)
  {
    os_mutexLock (&p->e.lock);
    x += cpf (conn, "proxypp %x:%x:%x:%x%s\n", PGUID (p->e.guid), p->is_ddsi2_pp ? " [ddsi2]" : "");
    os_mutexUnlock (&p->e.lock);
    x += print_addrset (conn, "  as data", p->as_default, "");
    x += print_addrset (conn, " meta", p->as_default, "\n");

    {
      struct ephash_enum_proxy_reader er;
      struct proxy_reader *r;
      ephash_enum_proxy_reader_init (&er);
      while ((r = ephash_enum_proxy_reader_next (&er)) != NULL)
      {
        ut_avlIter_t writ;
        struct prd_wr_match *m;
        if (r->c.proxypp != p)
          continue;
        os_mutexLock (&r->e.lock);
        print_proxy_endpoint_common (conn, "prd", &r->e, &r->c);
        for (m = ut_avlIterFirst (&rd_writers_treedef, &r->writers, &writ); m; m = ut_avlIterNext (&writ))
          x += cpf (conn, "    wr %x:%x:%x:%x\n", PGUID (m->wr_guid));
        os_mutexUnlock (&r->e.lock);
      }
      ephash_enum_proxy_reader_fini (&er);
    }

    {
      struct ephash_enum_proxy_writer ew;
      struct proxy_writer *w;
      ephash_enum_proxy_writer_init (&ew);
      while ((w = ephash_enum_proxy_writer_next (&ew)) != NULL)
      {
        ut_avlIter_t rdit;
        struct pwr_rd_match *m;
        if (w->c.proxypp != p)
          continue;
        os_mutexLock (&w->e.lock);
        print_proxy_endpoint_common (conn, "pwr", &w->e, &w->c);
        x += cpf (conn, "    last_seq %lld last_fragnum %u\n", w->last_seq, w->last_fragnum);
        for (m = ut_avlIterFirst (&wr_readers_treedef, &w->readers, &rdit); m; m = ut_avlIterNext (&rdit))
        {
          x += cpf (conn, "    rd %x:%x:%x:%x (nack %lld %lld)\n",
                    PGUID (m->rd_guid), m->seq_last_nack, m->t_last_nack);
          switch (m->in_sync)
          {
            case PRMSS_SYNC:
              break;
            case PRMSS_TLCATCHUP:
              x += cpf (conn, "      tl-catchup end_of_tl_seq %lld\n", m->u.not_in_sync.end_of_tl_seq);
              break;
            case PRMSS_OUT_OF_SYNC:
              x += cpf (conn, "      out-of-sync end_of_tl_seq %lld end_of_out_of_sync_seq %lld\n", m->u.not_in_sync.end_of_tl_seq, m->u.not_in_sync.end_of_out_of_sync_seq);
              break;
          }
        }
        os_mutexUnlock (&w->e.lock);
      }
      ephash_enum_proxy_writer_fini (&ew);
    }
  }
  ephash_enum_proxy_participant_fini (&e);
  thread_state_asleep (self);
  return x;
}

static uint32_t debmon_main (void *vdm)
{
  struct debug_monitor *dm = vdm;
  os_mutexLock (&dm->lock);
  while (!dm->stop)
  {
    ddsi_tran_conn_t conn;
    os_mutexUnlock (&dm->lock);
    if ((conn = ddsi_listener_accept (dm->servsock)) != NULL)
    {
      struct plugin *p;
      int r = 0;
      r += print_participants (dm->servts, conn);
      if (r == 0)
        r += print_proxy_participants (dm->servts, conn);

      /* Note: can only add plugins (at the tail) */
      os_mutexLock (&dm->lock);
      p = dm->plugins;
      while (r == 0 && p != NULL)
      {
        os_mutexUnlock (&dm->lock);
        r += p->fn (conn, cpf, p->arg);
        os_mutexLock (&dm->lock);
        p = p->next;
      }
      os_mutexUnlock (&dm->lock);

      ddsi_conn_free (conn);
    }
    os_mutexLock (&dm->lock);
  }
  os_mutexUnlock (&dm->lock);
  return 0;
}

struct debug_monitor *new_debug_monitor (int port)
{
  struct debug_monitor *dm;

  if (config.monitor_port < 0)
    return NULL;

  if (ddsi_tcp_init () < 0)
    return NULL;

  dm = os_malloc (sizeof (*dm));

  dm->plugins = NULL;
  if ((dm->tran_factory = ddsi_factory_find ("tcp")) == NULL)
    dm->tran_factory = ddsi_factory_find ("tcp6");
  dm->servsock = ddsi_factory_create_listener (dm->tran_factory, port, NULL);
  if (dm->servsock == NULL)
  {
    DDS_WARNING("debmon: can't create socket\n");
    goto err_servsock;
  }

  {
    nn_locator_t loc;
    char buf[DDSI_LOCSTRLEN];
    (void) ddsi_listener_locator(dm->servsock, &loc);
    DDS_LOG(DDS_LC_CONFIG, "debmon at %s\n", ddsi_locator_to_string (buf, sizeof(buf), &loc));
  }

  os_mutexInit (&dm->lock);
  os_condInit (&dm->cond, &dm->lock);
  if (ddsi_listener_listen (dm->servsock) < 0)
    goto err_listen;
  dm->stop = 0;
  dm->servts = create_thread("debmon", debmon_main, dm);
  return dm;

err_listen:
  os_condDestroy(&dm->cond);
  os_mutexDestroy(&dm->lock);
  ddsi_listener_free(dm->servsock);
err_servsock:
  os_free(dm);
  return NULL;
}

void add_debug_monitor_plugin (struct debug_monitor *dm, debug_monitor_plugin_t fn, void *arg)
{
  struct plugin *p, **pp;
  if (dm != NULL && (p = os_malloc (sizeof (*p))) != NULL)
  {
    p->fn = fn;
    p->arg = arg;
    p->next = NULL;
    os_mutexLock (&dm->lock);
    pp = &dm->plugins;
    while (*pp)
      pp = &(*pp)->next;
    *pp = p;
    os_mutexUnlock (&dm->lock);
  }
}

void free_debug_monitor (struct debug_monitor *dm)
{
  if (dm == NULL)
    return;

  os_mutexLock (&dm->lock);
  dm->stop = 1;
  os_condBroadcast (&dm->cond);
  os_mutexUnlock (&dm->lock);
  ddsi_listener_unblock (dm->servsock);
  join_thread (dm->servts);
  ddsi_listener_free (dm->servsock);
  os_condDestroy (&dm->cond);
  os_mutexDestroy (&dm->lock);

  while (dm->plugins) {
    struct plugin *p = dm->plugins;
    dm->plugins = p->next;
    os_free (p);
  }
  os_free (dm);
}

