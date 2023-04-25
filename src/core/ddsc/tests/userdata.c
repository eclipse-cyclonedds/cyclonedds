// Copyright(c) 2019 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dds/dds.h"

#include "dds/ddsrt/time.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/environ.h"

#include "test_common.h"
#include "RWData.h"

struct pp_thread_arg {
  dds_domainid_t domainid;
  bool master;
  unsigned ncycles;
};

static uint32_t pp_thread (void *varg)
{
  const struct pp_thread_arg *arg = varg;
#define prefix "ppuserdata:"
  static const char *exp_ud[] = {
    prefix "a", prefix "bc", prefix "def", prefix ""
  };
  const char *expud = arg->master ? prefix "X" : prefix;
  size_t expusz = strlen (expud);
  dds_entity_t dp, rd, ws;
  dds_instance_handle_t dpih;
  dds_return_t rc;
  dds_qos_t *qos;

  qos = dds_create_qos ();
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dds_qset_userdata (qos, expud, expusz);
  dp = dds_create_participant (arg->domainid, qos, NULL);
  CU_ASSERT_FATAL (dp > 0);
  rc = dds_get_instance_handle (dp, &dpih);
  CU_ASSERT_FATAL (rc == 0);
  rd = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, qos, NULL);
  CU_ASSERT_FATAL (rd > 0);
  rc = dds_set_status_mask (rd, DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  ws = dds_create_waitset (dp);
  CU_ASSERT_FATAL (ws > 0);
  rc = dds_waitset_attach (ws, rd, 0);
  CU_ASSERT_FATAL (rc == 0);

  bool done = false;
  bool synced = !arg->master;
  unsigned exp_index = 0;
  unsigned exp_cycle = 0;
  while (!done)
  {
    rc = dds_waitset_wait (ws, NULL, 0, DDS_INFINITY);
    CU_ASSERT_FATAL (rc >= 0);

    void *raw = NULL;
    dds_sample_info_t si;
    int32_t n;
    while ((n = dds_take (rd, &raw, &si, 1, 1)) == 1)
    {
      const dds_builtintopic_participant_t *sample = raw;
      if (si.instance_state != DDS_IST_ALIVE)
        done = true;
      else if (si.instance_handle == dpih || !si.valid_data)
        continue;
      else
      {
        void *ud = NULL;
        size_t usz = 0;
        if (!dds_qget_userdata (sample->qos, &ud, &usz))
        {
          CU_ASSERT_FATAL (0);
        }
        if (ud == NULL || strncmp (ud, prefix, sizeof (prefix) - 1) != 0)
        {
          /* presumably another process */
        }
        else if (!synced && strcmp (ud, expud) != 0)
        {
          /* slave hasn't discovered us yet */
        }
        else if (synced && arg->master && strcmp (ud, prefix "X") == 0 && exp_index == 1 && exp_cycle == 0)
        {
          /* FIXME: don't want no stutter of the initial sample ... */
        }
        else
        {
          synced = true;
          if (arg->master)
          {
            bool eq = (usz == expusz && (usz == 0 || memcmp (ud, expud, usz) == 0));
            printf ("expected %u %zu/%s received %zu/%s\n", exp_index, expusz, expud, usz, ud ? (char *) ud : "(null)");
            fflush (stdout);
            CU_ASSERT_FATAL (eq);
            if (++exp_index == sizeof (exp_ud) / sizeof (exp_ud[0]))
            {
              exp_index = 0;
              exp_cycle++;
            }

            if (exp_cycle == arg->ncycles)
              done = true;

            expud = exp_ud[exp_index];
            expusz = strlen (expud);
          }
          else
          {
            printf ("slave: received %zu/%s\n", usz, ud ? (char *) ud : "(null)");
            fflush (stdout);
            expud = ud;
            expusz = usz;
          }

          printf ("%s: set qos to %zu/%s\n", arg->master ? "master" : "slave", expusz, expud);
          fflush (stdout);
          dds_qset_userdata (qos, expud, expusz);
          rc = dds_set_qos (dp, qos);
          CU_ASSERT_FATAL (rc == 0);

          dds_qos_t *chk = dds_create_qos ();
          rc = dds_get_qos (dp, chk);
          CU_ASSERT_FATAL (rc == 0);

          void *chkud = NULL;
          size_t chkusz = 0;
          if (!dds_qget_userdata (chk, &chkud, &chkusz))
            CU_ASSERT_FATAL (0);
          CU_ASSERT_FATAL (chkusz == expusz && (expusz == 0 || memcmp (chkud, expud, expusz) == 0));
          dds_free (chkud);
          dds_delete_qos (chk);
        }
        dds_free (ud);
      }
    }
    CU_ASSERT_FATAL (n == 0);
    dds_return_loan (rd, &raw, 1);
    fflush (stdout);
  }
  dds_delete_qos (qos);
  rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
  return 0;
#undef prefix
}

CU_Test(ddsc_userdata, participant)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
   * in configuration is 0, so that both domains will map to the same port number.
   * This allows to create two domains in a single test process. */
  const char *config = "\
${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}\
<Discovery>\
  <ExternalDomainId>0</ExternalDomainId>\
  <Tag>\\${CYCLONEDDS_PID}</Tag>\
</Discovery>";
  char *master_conf = ddsrt_expand_envvars (config, 0);
  char *slave_conf = ddsrt_expand_envvars (config, 1);
  const dds_entity_t master_dom = dds_create_domain (0, master_conf);
  CU_ASSERT_FATAL (master_dom > 0);
  const dds_entity_t slave_dom = dds_create_domain (1, slave_conf);
  CU_ASSERT_FATAL (slave_dom > 0);
  ddsrt_free (master_conf);
  ddsrt_free (slave_conf);

  ddsrt_threadattr_t tattr;
  ddsrt_threadattr_init (&tattr);

  ddsrt_thread_t master_tid, slave_tid;
  dds_return_t rc;

  const unsigned ncycles = 10;
  struct pp_thread_arg master_arg = {
    .domainid = 1,
    .master = true,
    .ncycles = ncycles
  };
  rc = ddsrt_thread_create (&master_tid, "master_thread", &tattr, pp_thread, &master_arg);
  CU_ASSERT_FATAL (rc == 0);

  struct pp_thread_arg slave_arg = {
    .domainid = 0,
    .master = false,
    .ncycles = ncycles
  };
  rc = ddsrt_thread_create (&slave_tid, "slave_thread", &tattr, pp_thread, &slave_arg);
  CU_ASSERT_FATAL (rc == 0);

  ddsrt_thread_join (master_tid, NULL);
  ddsrt_thread_join (slave_tid, NULL);

  rc = dds_delete (master_dom);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_delete (slave_dom);
  CU_ASSERT_FATAL (rc == 0);
}

enum rwud {
  RWUD_USERDATA,
  RWUD_GROUPDATA,
  RWUD_TOPICDATA
};

struct rw_thread_arg {
  dds_domainid_t domainid;
  const char *topicname;
  bool master;
  unsigned ncycles;
  enum rwud which;
};

static uint32_t rw_thread (void *varg)
{
  struct rw_thread_arg *arg = varg;
  static const char *exp_rwud[] = {
    "a", "bc", "def", "",
  };
  bool (*qget) (const dds_qos_t * __restrict qos, void **value, size_t *sz) = 0;
  void (*qset) (dds_qos_t * __restrict qos, const void *value, size_t sz) = 0;

  dds_entity_t dp, tp, ep, grp, rdep, qent = 0, ws;
  dds_return_t rc;
  dds_qos_t *qos;

  const char *expud = arg->master ? "X" : "";
  size_t expusz = strlen (expud);

  qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dp = dds_create_participant (arg->domainid, NULL, NULL);
  CU_ASSERT_FATAL (dp > 0);
  tp = dds_create_topic (dp, &RWData_Msg_desc, arg->topicname, qos, NULL);
  CU_ASSERT_FATAL (tp > 0);
  if (arg->master)
  {
    rdep = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, qos, NULL);
    CU_ASSERT_FATAL (rdep > 0);
    grp = dds_create_publisher (dp, qos, NULL);
    CU_ASSERT_FATAL (grp > 0);
    ep = dds_create_writer (grp, tp, qos, NULL);
    CU_ASSERT_FATAL (ep > 0);
  }
  else
  {
    rdep = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, qos, NULL);
    CU_ASSERT_FATAL (rdep > 0);
    grp = dds_create_subscriber (dp, qos, NULL);
    CU_ASSERT_FATAL (grp > 0);
    ep = dds_create_reader (grp, tp, qos, NULL);
    CU_ASSERT_FATAL (ep > 0);
  }
  rc = dds_set_status_mask (rdep, DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  ws = dds_create_waitset (dp);
  CU_ASSERT_FATAL (ws > 0);
  rc = dds_waitset_attach (ws, rdep, 0);
  CU_ASSERT_FATAL (rc == 0);

  switch (arg->which)
  {
    case RWUD_USERDATA:
      qget = dds_qget_userdata;
      qset = dds_qset_userdata;
      qent = ep;
      break;
    case RWUD_GROUPDATA:
      qget = dds_qget_groupdata;
      qset = dds_qset_groupdata;
      qent = grp;
      CU_ASSERT_FATAL (qent > 0);
      break;
    case RWUD_TOPICDATA:
      qget = dds_qget_topicdata;
      qset = dds_qset_topicdata;
      qent = tp;
      break;
  }

  if (arg->master)
  {
    qset (qos, expud, expusz);
    rc = dds_set_qos (qent, qos);
    CU_ASSERT_FATAL (rc == 0);
  }

  bool done = false;
  bool synced = !arg->master;
  unsigned exp_index = 0;
  unsigned exp_cycle = 0;
  dds_instance_handle_t peer = 0;
  while (!done)
  {
    rc = dds_waitset_wait (ws, NULL, 0, DDS_INFINITY);
    CU_ASSERT_FATAL (rc >= 0);

    void *raw = NULL;
    dds_sample_info_t si;
    int32_t n;
    while ((n = dds_take (rdep, &raw, &si, 1, 1)) == 1)
    {
      const dds_builtintopic_endpoint_t *sample = raw;
      if (si.instance_state != DDS_IST_ALIVE && si.instance_handle == peer)
        done = true;
      else if (!si.valid_data)
        continue;
      else if ((peer && si.instance_handle != peer) || strcmp (sample->topic_name, arg->topicname) != 0)
        continue;
      else
      {
        void *ud = NULL;
        size_t usz = 0;
        if (!qget (sample->qos, &ud, &usz))
        {
          CU_ASSERT_FATAL (0);
        }
        else if (!synced && (ud == NULL || strcmp (ud, expud) != 0))
        {
          /* slave hasn't discovered us yet */
        }
        else
        {
          peer = si.instance_handle;
          synced = true;
          if (arg->master)
          {
            bool eq = (usz == expusz && (usz == 0 || memcmp (ud, expud, usz) == 0));
            printf ("expected %u %zu/%s received %zu/%s\n", exp_index, expusz, expud, usz, ud ? (char *) ud : "(null)");
            CU_ASSERT_FATAL (eq);
            if (++exp_index == sizeof (exp_rwud) / sizeof (exp_rwud[0]))
            {
              exp_index = 0;
              exp_cycle++;
            }

            if (exp_cycle == arg->ncycles)
              done = true;

            expud = exp_rwud[exp_index];
            expusz = strlen (expud);
          }
          else
          {
            printf ("slave: received %zu/%s\n", usz, ud ? (char *) ud : "(null)");
            expud = ud;
            expusz = usz;
          }

          qset (qos, expud, expusz);
          rc = dds_set_qos (qent, qos);
          CU_ASSERT_FATAL (rc == 0);

          dds_qos_t *chk = dds_create_qos ();
          rc = dds_get_qos (ep, chk);
          CU_ASSERT_FATAL (rc == 0);

          void *chkud = NULL;
          size_t chkusz = 0;
          if (!qget (chk, &chkud, &chkusz))
            CU_ASSERT_FATAL (0);
          CU_ASSERT_FATAL (chkusz == expusz && (expusz == 0 || (chkud != NULL && expud != NULL && memcmp (chkud, expud, expusz) == 0)));
          dds_free (chkud);
          dds_delete_qos (chk);
        }
        dds_free (ud);
      }
    }
    CU_ASSERT_FATAL (n == 0);
    dds_return_loan (rdep, &raw, 1);
    fflush (stdout);
  }
  dds_delete_qos (qos);

  rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
  return 0;
}

static void rw_test (enum rwud which)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
   * in configuration is 0, so that both domains will map to the same port number.
   * This allows to create two domains in a single test process. */
  const char *config = "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>";
  char *master_conf = ddsrt_expand_envvars (config, 0);
  char *slave_conf = ddsrt_expand_envvars (config, 1);
  const dds_entity_t master_dom = dds_create_domain (0, master_conf);
  CU_ASSERT_FATAL (master_dom > 0);
  const dds_entity_t slave_dom = dds_create_domain (1, slave_conf);
  CU_ASSERT_FATAL (slave_dom > 0);
  ddsrt_free (master_conf);
  ddsrt_free (slave_conf);

  char topicname[100];
  create_unique_topic_name ("ddsc_userdata", topicname, sizeof topicname);

  ddsrt_threadattr_t tattr;
  ddsrt_threadattr_init (&tattr);

  ddsrt_thread_t master_tid, slave_tid;
  dds_return_t rc;

  const unsigned ncycles = 10;
  struct rw_thread_arg master_arg = {
    .domainid = 0,
    .topicname = topicname,
    .master = true,
    .ncycles = ncycles,
    .which = which
  };
  rc = ddsrt_thread_create (&master_tid, "master_thread", &tattr, rw_thread, &master_arg);
  CU_ASSERT_FATAL (rc == 0);

  struct rw_thread_arg slave_arg = {
    .domainid = 1,
    .topicname = topicname,
    .master = false,
    .ncycles = ncycles,
    .which = which
  };
  rc = ddsrt_thread_create (&slave_tid, "slave_thread", &tattr, rw_thread, &slave_arg);
  CU_ASSERT_FATAL (rc == 0);

  ddsrt_thread_join (master_tid, NULL);
  ddsrt_thread_join (slave_tid, NULL);

  rc = dds_delete (master_dom);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_delete (slave_dom);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test(ddsc_userdata, endpoint)
{
  rw_test (RWUD_USERDATA);
}

CU_Test(ddsc_userdata, group)
{
  rw_test (RWUD_GROUPDATA);
}

CU_Test(ddsc_userdata, topic)
{
  rw_test (RWUD_TOPICDATA);
}

