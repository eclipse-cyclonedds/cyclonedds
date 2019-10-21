/*
 * Copyright(c) 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mpt/mpt.h"

#include "cyclonedds/dds.h"

#include "cyclonedds/ddsrt/time.h"
#include "cyclonedds/ddsrt/threads.h"
#include "cyclonedds/ddsrt/sync.h"
#include "cyclonedds/ddsrt/process.h"
#include "cyclonedds/ddsrt/sockets.h"
#include "cyclonedds/ddsrt/heap.h"

#include "ppud.h"
#include "rwdata.h"

void ppud_init (void) { }
void ppud_fini (void) { }

MPT_ProcessEntry (ppud,
                  MPT_Args (dds_domainid_t domainid,
                            bool master,
                            unsigned ncycles))
{
#define prefix "ppuserdata:"
  static const char *exp_ud[] = {
    prefix "a", prefix "bc", prefix "def", prefix ""
  };
  const char *expud = master ? prefix "X" : prefix;
  size_t expusz = strlen (expud);
  dds_entity_t dp, rd, ws;
  dds_instance_handle_t dpih;
  dds_return_t rc;
  dds_qos_t *qos;
  int id = (int) ddsrt_getpid ();

  printf ("=== [Check(%d)] master=%d ncycles=%u Start(%d) ...\n", id, master, ncycles, (int) domainid);

  qos = dds_create_qos ();
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dds_qset_userdata (qos, expud, expusz);
  dp = dds_create_participant (domainid, qos, NULL);
  MPT_ASSERT_FATAL_GT (dp, 0, "Could not create participant: %s\n", dds_strretcode (dp));
  rc = dds_get_instance_handle (dp, &dpih);
  MPT_ASSERT_FATAL_EQ (rc, 0, "Could not get participant instance handle: %s\n", dds_strretcode (rc));
  rd = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, qos, NULL);
  MPT_ASSERT_FATAL_GT (rd, 0, "Could not create DCPSParticipant reader: %s\n", dds_strretcode (rd));
  rc = dds_set_status_mask (rd, DDS_DATA_AVAILABLE_STATUS);
  MPT_ASSERT_FATAL_EQ (rc, 0, "Could not set status mask: %s\n", dds_strretcode (rc));
  ws = dds_create_waitset (dp);
  MPT_ASSERT_FATAL_GT (ws, 0, "Could not create waitset: %s\n", dds_strretcode (ws));
  rc = dds_waitset_attach (ws, rd, 0);
  MPT_ASSERT_FATAL_EQ (rc, 0, "Could not attach reader to waitset: %s\n", dds_strretcode (rc));

  bool done = false;
  bool synced = !master;
  unsigned exp_index = 0;
  unsigned exp_cycle = 0;
  while (!done)
  {
    rc = dds_waitset_wait (ws, NULL, 0, DDS_INFINITY);
    MPT_ASSERT_FATAL_GEQ (rc, 0, "Wait failed: %s\n", dds_strretcode (ws));

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
          MPT_ASSERT (0, "%d: user data not set in QoS\n", id);
        if (ud == NULL || strncmp (ud, prefix, sizeof (prefix) - 1) != 0)
        {
          /* presumably another process */
        }
        else if (!synced && strcmp (ud, expud) != 0)
        {
          /* slave hasn't discovered us yet */
        }
        else if (synced && master && strcmp (ud, prefix "X") == 0 && exp_index == 1 && exp_cycle == 0)
        {
          /* FIXME: don't want no stutter of the initial sample ... */
        }
        else
        {
          synced = true;
          if (master)
          {
            bool eq = (usz == expusz && (usz == 0 || memcmp (ud, expud, usz) == 0));
            printf ("%d: expected %u %zu/%s received %zu/%s\n",
                    id, exp_index, expusz, expud, usz, ud ? (char *) ud : "(null)");
            MPT_ASSERT (eq, "user data mismatch: expected %u %zu/%s received %zu/%s\n",
                        exp_index, expusz, expud ? expud : "(null)", usz, ud ? (char *) ud : "(null)");
            if (++exp_index == sizeof (exp_ud) / sizeof (exp_ud[0]))
            {
              exp_index = 0;
              exp_cycle++;
            }

            if (exp_cycle == ncycles)
              done = true;

            expud = exp_ud[exp_index];
            expusz = strlen (expud);
          }
          else
          {
            printf ("%d: slave: received %zu/%s\n", id, usz, ud ? (char *) ud : "(null)");
            expud = ud;
            expusz = usz;
          }

          dds_qset_userdata (qos, expud, expusz);
          rc = dds_set_qos (dp, qos);
          MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Set QoS failed: %s\n", dds_strretcode (rc));

          dds_qos_t *chk = dds_create_qos ();
          rc = dds_get_qos (dp, chk);
          MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Get QoS failed: %s\n", dds_strretcode (rc));

          void *chkud = NULL;
          size_t chkusz = 0;
          if (!dds_qget_userdata (chk, &chkud, &chkusz))
            MPT_ASSERT (0, "Check QoS: no user data present\n");
          MPT_ASSERT (chkusz == expusz && (expusz == 0 || memcmp (chkud, expud, expusz) == 0),
                      "Retrieved user data differs from user data just set (%zu/%s vs %zu/%s)\n",
                      chkusz, chkud ? (char *) chkud : "(null)", expusz, expud ? (char *) expud : "(null)");
          dds_free (chkud);
          dds_delete_qos (chk);
        }
        dds_free (ud);
      }
    }
    MPT_ASSERT_FATAL_EQ (n, 0, "Read failed: %s\n", dds_strretcode (n));
    dds_return_loan (rd, &raw, 1);
    fflush (stdout);
  }
  dds_delete_qos (qos);
  rc = dds_delete (dp);
  MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "teardown failed\n");
  printf ("=== [Check(%d)] Done\n", id);
#undef prefix
}

static const char *exp_rwud[2][4] = {
  { "a", "bc", "def", "" },
  { "p", "qr", "stu", "" }
};

struct rwud_barrier {
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  int initcount;
  int count;
};

static void barrierwait (struct rwud_barrier *barrier, int id)
{
  printf ("%d waiting at barrier\n", id);
  fflush (stdout);
  ddsrt_mutex_lock (&barrier->lock);
  assert (barrier->initcount > 0);
  if (barrier->count == 0)
    barrier->count = barrier->initcount;
  if (--barrier->count == 0)
    ddsrt_cond_broadcast (&barrier->cond);
  while (barrier->count > 0)
    ddsrt_cond_wait (&barrier->cond, &barrier->lock);
  ddsrt_mutex_unlock (&barrier->lock);
  printf ("%d continuing past barrier\n", id);
  fflush (stdout);
}

MPT_ProcessEntry (rwud,
                  MPT_Args (dds_domainid_t domainid,
                            const char *topic_name,
                            bool master,
                            unsigned ncycles,
                            enum rwud which,
                            struct rwud_barrier *barrier))
{
  bool (*qget) (const dds_qos_t * __restrict qos, void **value, size_t *sz) = 0;
  void (*qset) (dds_qos_t * __restrict qos, const void *value, size_t sz) = 0;
  const char *qname = "UNDEFINED";

  dds_entity_t dp, tp, ep, grp, rdep, qent = 0, ws;
  dds_return_t rc;
  dds_qos_t *qos;
  int id = (int) ddsrt_getpid ();

  const char *expud = master ? "X" : "";
  size_t expusz = strlen (expud);

  printf ("=== [Check(%d)] master=%d ncycles=%u Start(%d) ...\n", id, master, ncycles, (int) domainid);

  qos = dds_create_qos ();
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dp = dds_create_participant (domainid, NULL, NULL);
  MPT_ASSERT_FATAL_GT (dp, 0, "Could not create participant: %s\n", dds_strretcode (dp));
  tp = dds_create_topic (dp, &RWData_Msg_desc, topic_name, qos, NULL);
  MPT_ASSERT_FATAL_GT (tp, 0, "Could not create topic: %s\n", dds_strretcode (tp));
  if (master)
  {
    rdep = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, qos, NULL);
    MPT_ASSERT_FATAL_GT (rdep, 0, "Could not create DCPSSubscription reader: %s\n", dds_strretcode (rdep));
    grp = dds_create_publisher (dp, qos, NULL);
    MPT_ASSERT_FATAL_GT (grp, 0, "Could not create publisher: %s\n", dds_strretcode (grp));
    ep = dds_create_writer (grp, tp, qos, NULL);
    MPT_ASSERT_FATAL_GT (ep, 0, "Could not create writer: %s\n", dds_strretcode (ep));
  }
  else
  {
    rdep = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, qos, NULL);
    MPT_ASSERT_FATAL_GT (rdep, 0, "Could not create DCPSPublication reader: %s\n", dds_strretcode (rdep));
    grp = dds_create_subscriber (dp, qos, NULL);
    MPT_ASSERT_FATAL_GT (grp, 0, "Could not create subscriber: %s\n", dds_strretcode (grp));
    ep = dds_create_reader (grp, tp, qos, NULL);
    MPT_ASSERT_FATAL_GT (ep, 0, "Could not create reader: %s\n", dds_strretcode (ep));
  }
  rc = dds_set_status_mask (rdep, DDS_DATA_AVAILABLE_STATUS);
  MPT_ASSERT_FATAL_EQ (rc, 0, "Could not set status mask: %s\n", dds_strretcode (rc));
  ws = dds_create_waitset (dp);
  MPT_ASSERT_FATAL_GT (ws, 0, "Could not create waitset: %s\n", dds_strretcode (ws));
  rc = dds_waitset_attach (ws, rdep, 0);
  MPT_ASSERT_FATAL_EQ (rc, 0, "Could not attach built-in reader to waitset: %s\n", dds_strretcode (rc));

  switch (which)
  {
    case RWUD_USERDATA:
      qget = dds_qget_userdata;
      qset = dds_qset_userdata;
      qname = "user data";
      qent = ep;
      break;
    case RWUD_GROUPDATA:
      qget = dds_qget_groupdata;
      qset = dds_qset_groupdata;
      qname = "group data";
      qent = grp;
      MPT_ASSERT_FATAL_GT (qent, 0, "Could not get pub/sub from wr/rd: %s\n", dds_strretcode (qent));
      break;
    case RWUD_TOPICDATA:
      qget = dds_qget_topicdata;
      qset = dds_qset_topicdata;
      qname = "topic data";
      qent = tp;
      break;
  }

  if (master)
  {
    qset (qos, expud, expusz);
    rc = dds_set_qos (qent, qos);
    MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Set QoS failed: %s\n", dds_strretcode (rc));
  }

  if (barrier)
  {
    barrierwait (barrier, id);
  }

  bool done = false;
  bool synced = !master;
  const unsigned exp_setindex = (unsigned) domainid % 2;
  unsigned exp_index = 0;
  unsigned exp_cycle = 0;
  dds_instance_handle_t peer = 0;
  while (!done)
  {
    rc = dds_waitset_wait (ws, NULL, 0, DDS_INFINITY);
    MPT_ASSERT_FATAL_GEQ (rc, 0, "Wait failed: %s\n", dds_strretcode (ws));

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
      else if ((peer && si.instance_handle != peer) || strcmp (sample->topic_name, topic_name) != 0)
        continue;
      else
      {
        void *ud = NULL;
        size_t usz = 0;
        if (!qget (sample->qos, &ud, &usz))
          MPT_ASSERT (0, "%d: %s not set in QoS\n", id, qname);
        else if (!synced && (ud == NULL || strcmp (ud, expud) != 0))
        {
          /* slave hasn't discovered us yet */
        }
        else
        {
          peer = si.instance_handle;
          synced = true;
          if (master)
          {
            bool eq = (usz == expusz && (usz == 0 || memcmp (ud, expud, usz) == 0));
            printf ("%d: expected %u %zu/%s received %zu/%s\n",
                    id, exp_index, expusz, expud, usz, ud ? (char *) ud : "(null)");
            MPT_ASSERT (eq, "%s mismatch: expected %u %zu/%s received %zu/%s\n",
                        qname, exp_index, expusz, expud ? expud : "(null)", usz, ud ? (char *) ud : "(null)");
            if (++exp_index == sizeof (exp_rwud[0]) / sizeof (exp_rwud[0][0]))
            {
              exp_index = 0;
              exp_cycle++;
            }

            if (exp_cycle == ncycles)
              done = true;

            expud = exp_rwud[exp_setindex][exp_index];
            expusz = strlen (expud);
          }
          else
          {
            printf ("%d: slave: received %zu/%s\n", id, usz, ud ? (char *) ud : "(null)");
            expud = ud;
            expusz = usz;
          }

          qset (qos, expud, expusz);
          rc = dds_set_qos (qent, qos);
          MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Set QoS failed: %s\n", dds_strretcode (rc));

          dds_qos_t *chk = dds_create_qos ();
          rc = dds_get_qos (ep, chk);
          MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Get QoS failed: %s\n", dds_strretcode (rc));

          void *chkud = NULL;
          size_t chkusz = 0;
          if (!qget (chk, &chkud, &chkusz))
            MPT_ASSERT (0, "Check QoS: no %s present\n", qname);
          MPT_ASSERT (chkusz == expusz && (expusz == 0 || (chkud != NULL && memcmp (chkud, expud, expusz) == 0)),
                      "Retrieved %s differs from group data just set (%zu/%s vs %zu/%s)\n", qname,
                      chkusz, chkud ? (char *) chkud : "(null)", expusz, expud ? (char *) expud : "(null)");
          dds_free (chkud);
          dds_delete_qos (chk);
        }
        dds_free (ud);
      }
    }
    MPT_ASSERT_FATAL_EQ (n, 0, "Read failed: %s\n", dds_strretcode (n));
    dds_return_loan (rdep, &raw, 1);
    fflush (stdout);
  }
  dds_delete_qos (qos);

  if (barrier)
  {
    barrierwait (barrier, id);
  }

  rc = dds_delete (dp);
  MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "teardown failed\n");
  printf ("=== [Check(%d)] Done\n", id);
}

struct rwudM_thread_arg {
  dds_domainid_t domainid;
  const char *topic_name;
  bool master;
  unsigned ncycles;
  enum rwud which;
  mpt_retval_t retval;
  struct rwud_barrier *barrier;
  const mpt_data_t *mpt__args__;
};

static uint32_t rwudM_thread (void *varg)
{
  struct rwudM_thread_arg *arg = varg;
  const mpt_data_t *mpt__args__ = arg->mpt__args__;
  mpt_retval_t *mpt__retval__ = &arg->retval;
  MPT_ProcessEntryName(rwud) (MPT_ArgValues (arg->domainid, arg->topic_name, arg->master, arg->ncycles, arg->which, arg->barrier));
  return 0;
}

MPT_ProcessEntry (rwudM,
                  MPT_Args (dds_domainid_t domainid,
                            const char *topic_name,
                            bool master,
                            unsigned ncycles,
                            enum rwud which))
{
  dds_return_t ret;
  uint32_t dummy;
  ddsrt_thread_t thr[2];
  ddsrt_threadattr_t attr;
  struct rwud_barrier barrier;
  struct rwudM_thread_arg a = {
    .domainid = domainid,
    .topic_name = topic_name,
    .master = master,
    .ncycles = ncycles,
    .which = which,
    .barrier = &barrier,
    .retval = MPT_SUCCESS,
    .mpt__args__ = mpt__args__
  };
  struct rwudM_thread_arg b;
  b = a; ++b.domainid;

  ddsrt_mutex_init (&barrier.lock);
  ddsrt_cond_init (&barrier.cond);
  barrier.initcount = 2;
  barrier.count = 0;

  ddsrt_threadattr_init (&attr);
  ret = ddsrt_thread_create (&thr[0], "a", &attr, &rwudM_thread, &a);
  MPT_ASSERT_FATAL_EQ (ret, DDS_RETCODE_OK, "failed to create thread a\n");
  ret = ddsrt_thread_create (&thr[1], "b", &attr, &rwudM_thread, &b);
  MPT_ASSERT_FATAL_EQ (ret, DDS_RETCODE_OK, "failed to create thread b\n");
  ret = ddsrt_thread_join (thr[0], &dummy);
  MPT_ASSERT_FATAL_EQ (ret, DDS_RETCODE_OK, "failed to join thread a\n");
  ret = ddsrt_thread_join (thr[1], &dummy);
  MPT_ASSERT_FATAL_EQ (ret, DDS_RETCODE_OK, "failed to join thread b\n");
  /* forward thread failures to process failures */
  MPT_ASSERT_EQ (a.retval, MPT_SUCCESS, "thread a failed\n");
  MPT_ASSERT_EQ (b.retval, MPT_SUCCESS, "thread b failed\n");

  ddsrt_cond_destroy (&barrier.cond);
  ddsrt_mutex_destroy (&barrier.lock);
}
