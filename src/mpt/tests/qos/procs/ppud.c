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

#include "dds/dds.h"

#include "dds/ddsrt/time.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/heap.h"

#include "ppud.h"
#include "rwdata.h"

void ppud_init (void) { }
void ppud_fini (void) { }

static const char *exp_ud[] = {
  "a", "bc", "def", ""
};

MPT_ProcessEntry (ppud,
                  MPT_Args (dds_domainid_t domainid,
                            bool active,
                            unsigned ncycles))
{
  dds_entity_t dp, rd, ws;
  dds_instance_handle_t dpih;
  dds_return_t rc;
  dds_qos_t *qos;
  int id = (int) ddsrt_getpid ();

  printf ("=== [Check(%d)] active=%d ncycles=%u Start(%d) ...\n", id, active, ncycles, (int) domainid);

  qos = dds_create_qos ();
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
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
  bool first = true;
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
          printf ("%d: user data not set in QoS\n", id);
        if (first && usz == 0)
        {
          dds_qset_userdata (qos, "X", 1);
          rc = dds_set_qos (dp, qos);
          MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Set QoS failed: %s\n", dds_strretcode (rc));
        }
        else
        {
          const char *exp = exp_ud[exp_index];
          if (first && strcmp (ud, "X") == 0)
            exp = "X";
          const size_t expsz = strlen (exp);
          bool eq = (usz == expsz && (usz == 0 || memcmp (ud, exp, usz) == 0));
          //printf ("%d: expected %u %zu/%s received %zu/%s\n",
          //        id, exp_index, expsz, exp, usz, ud ? (char *) ud : "(null)");
          MPT_ASSERT (eq, "User data mismatch: expected %u %zu/%s received %zu/%s\n",
                      exp_index, expsz, exp, usz, ud ? (char *) ud : "(null)");
          if (strcmp (exp, "X") != 0 && ++exp_index == sizeof (exp_ud) / sizeof (exp_ud[0]))
          {
            exp_index = 0;
            exp_cycle++;
          }

          if (active && exp_cycle == ncycles)
            done = true;
          else
          {
            const void *newud;
            size_t newusz;
            if (!active)
            {
              /* Set user data to the same value in response */
              newud = ud; newusz = usz;
              dds_qset_userdata (qos, ud, usz);
            }
            else /* Set next agreed value */
            {
              newud = exp_ud[exp_index]; newusz = strlen (exp_ud[exp_index]);
              dds_qset_userdata (qos, newud, newusz);
            }

            rc = dds_set_qos (dp, qos);
            MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Set QoS failed: %s\n", dds_strretcode (rc));

            dds_qos_t *chk = dds_create_qos ();
            rc = dds_get_qos (dp, chk);
            MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Get QoS failed: %s\n", dds_strretcode (rc));

            void *chkud = NULL;
            size_t chkusz = 0;
            if (!dds_qget_userdata (chk, &chkud, &chkusz))
              MPT_ASSERT (0, "Check QoS: no user data present\n");
            MPT_ASSERT (chkusz == newusz && (newusz == 0 || memcmp (chkud, newud, newusz) == 0),
                        "Retrieved user data differs from user data just set (%zu/%s vs %zu/%s)\n",
                        chkusz, chkud ? (char *) chkud : "(null)", newusz, newud ? (char *) newud : "(null)");
            dds_free (chkud);
            dds_delete_qos (chk);
            first = false;
          }
        }
        dds_free (ud);
      }
    }
    MPT_ASSERT_FATAL_EQ (n, 0, "Read failed: %s\n", dds_strretcode (n));
    dds_return_loan (rd, &raw, 1);
  }
  dds_delete_qos (qos);
  rc = dds_delete (dp);
  MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "teardown failed\n");
  printf ("=== [Publisher(%d)] Done\n", id);
}

MPT_ProcessEntry (rwud,
                  MPT_Args (dds_domainid_t domainid,
                            const char *topic_name,
                            bool active,
                            unsigned ncycles))
{
  dds_entity_t dp, tp, ep, rdep, ws;
  dds_return_t rc;
  dds_qos_t *qos;
  int id = (int) ddsrt_getpid ();

  printf ("=== [Check(%d)] active=%d ncycles=%u Start(%d) ...\n", id, active, ncycles, (int) domainid);

  qos = dds_create_qos ();
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dp = dds_create_participant (domainid, NULL, NULL);
  MPT_ASSERT_FATAL_GT (dp, 0, "Could not create participant: %s\n", dds_strretcode (dp));
  tp = dds_create_topic (dp, &RWData_Msg_desc, topic_name, qos, NULL);
  MPT_ASSERT_FATAL_GT (tp, 0, "Could not create topic: %s\n", dds_strretcode (tp));
  if (active)
  {
    rdep = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, qos, NULL);
    MPT_ASSERT_FATAL_GT (rdep, 0, "Could not create DCPSSubscription reader: %s\n", dds_strretcode (rdep));
    ep = dds_create_writer (dp, tp, qos, NULL);
    MPT_ASSERT_FATAL_GT (ep, 0, "Could not create writer: %s\n", dds_strretcode (ep));
  }
  else
  {
    rdep = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, qos, NULL);
    MPT_ASSERT_FATAL_GT (rdep, 0, "Could not create DCPSPublication reader: %s\n", dds_strretcode (rdep));
    ep = dds_create_reader (dp, tp, qos, NULL);
    MPT_ASSERT_FATAL_GT (ep, 0, "Could not create reader: %s\n", dds_strretcode (ep));
  }
  rc = dds_set_status_mask (rdep, DDS_DATA_AVAILABLE_STATUS);
  MPT_ASSERT_FATAL_EQ (rc, 0, "Could not set status mask: %s\n", dds_strretcode (rc));
  ws = dds_create_waitset (dp);
  MPT_ASSERT_FATAL_GT (ws, 0, "Could not create waitset: %s\n", dds_strretcode (ws));
  rc = dds_waitset_attach (ws, rdep, 0);
  MPT_ASSERT_FATAL_EQ (rc, 0, "Could not attach built-in reader to waitset: %s\n", dds_strretcode (rc));

  bool done = false;
  bool first = true;
  unsigned exp_index = 0;
  unsigned exp_cycle = 0;
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
      if (si.instance_state != DDS_IST_ALIVE)
        done = true;
      else if (!si.valid_data || strcmp (sample->topic_name, topic_name) != 0)
        continue;
      else
      {
        void *ud = NULL;
        size_t usz = 0;
        if (!dds_qget_userdata (sample->qos, &ud, &usz))
          printf ("%d: user data not set in QoS\n", id);
        if (first && usz == 0)
        {
          dds_qset_userdata (qos, "X", 1);
          rc = dds_set_qos (ep, qos);
          MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Set QoS failed: %s\n", dds_strretcode (rc));
        }
        else
        {
          const char *exp = exp_ud[exp_index];
          if (first && strcmp (ud, "X") == 0)
            exp = "X";
          const size_t expsz = first ? 1 : strlen (exp);
          bool eq = (usz == expsz && (usz == 0 || memcmp (ud, exp, usz) == 0));
          //printf ("%d: expected %u %zu/%s received %zu/%s\n",
          //        id, exp_index, expsz, exp, usz, ud ? (char *) ud : "(null)");
          MPT_ASSERT (eq, "User data mismatch: expected %u %zu/%s received %zu/%s\n",
                      exp_index, expsz, exp, usz, ud ? (char *) ud : "(null)");
          if (strcmp (exp, "X") != 0 && ++exp_index == sizeof (exp_ud) / sizeof (exp_ud[0]))
          {
            exp_index = 0;
            exp_cycle++;
          }

          if (active && exp_cycle == ncycles)
            done = true;
          else
          {
            const void *newud;
            size_t newusz;
            if (!active)
            {
              /* Set user data to the same value in response */
              newud = ud; newusz = usz;
              dds_qset_userdata (qos, ud, usz);
            }
            else /* Set next agreed value */
            {
              newud = exp_ud[exp_index]; newusz = strlen (exp_ud[exp_index]);
              dds_qset_userdata (qos, newud, newusz);
            }

            rc = dds_set_qos (ep, qos);
            MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Set QoS failed: %s\n", dds_strretcode (rc));

            dds_qos_t *chk = dds_create_qos ();
            rc = dds_get_qos (ep, chk);
            MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Get QoS failed: %s\n", dds_strretcode (rc));

            void *chkud = NULL;
            size_t chkusz = 0;
            if (!dds_qget_userdata (chk, &chkud, &chkusz))
              MPT_ASSERT (0, "Check QoS: no user data present\n");
            MPT_ASSERT (chkusz == newusz && (newusz == 0 || memcmp (chkud, newud, newusz) == 0),
                        "Retrieved user data differs from user data just set (%zu/%s vs %zu/%s)\n",
                        chkusz, chkud ? (char *) chkud : "(null)", newusz, newud ? (char *) newud : "(null)");
            dds_free (chkud);
            dds_delete_qos (chk);
            first = false;
          }
        }
        dds_free (ud);
      }
    }
    MPT_ASSERT_FATAL_EQ (n, 0, "Read failed: %s\n", dds_strretcode (n));
    dds_return_loan (rdep, &raw, 1);
  }
  dds_delete_qos (qos);
  rc = dds_delete (dp);
  MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "teardown failed\n");
  printf ("=== [Publisher(%d)] Done\n", id);
}

MPT_ProcessEntry (rwtd,
                  MPT_Args (dds_domainid_t domainid,
                            const char *topic_name,
                            bool active,
                            unsigned ncycles))
{
  dds_entity_t dp, tp, ep, rdep, ws;
  dds_return_t rc;
  dds_qos_t *qos;
  int id = (int) ddsrt_getpid ();

  printf ("=== [Check(%d)] active=%d ncycles=%u Start(%d) ...\n", id, active, ncycles, (int) domainid);

  qos = dds_create_qos ();
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dp = dds_create_participant (domainid, NULL, NULL);
  MPT_ASSERT_FATAL_GT (dp, 0, "Could not create participant: %s\n", dds_strretcode (dp));
  tp = dds_create_topic (dp, &RWData_Msg_desc, topic_name, qos, NULL);
  MPT_ASSERT_FATAL_GT (tp, 0, "Could not create topic: %s\n", dds_strretcode (tp));
  if (active)
  {
    rdep = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, qos, NULL);
    MPT_ASSERT_FATAL_GT (rdep, 0, "Could not create DCPSSubscription reader: %s\n", dds_strretcode (rdep));
    ep = dds_create_writer (dp, tp, qos, NULL);
    MPT_ASSERT_FATAL_GT (ep, 0, "Could not create writer: %s\n", dds_strretcode (ep));
  }
  else
  {
    rdep = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, qos, NULL);
    MPT_ASSERT_FATAL_GT (rdep, 0, "Could not create DCPSPublication reader: %s\n", dds_strretcode (rdep));
    ep = dds_create_reader (dp, tp, qos, NULL);
    MPT_ASSERT_FATAL_GT (ep, 0, "Could not create reader: %s\n", dds_strretcode (ep));
  }
  rc = dds_set_status_mask (rdep, DDS_DATA_AVAILABLE_STATUS);
  MPT_ASSERT_FATAL_EQ (rc, 0, "Could not set status mask: %s\n", dds_strretcode (rc));
  ws = dds_create_waitset (dp);
  MPT_ASSERT_FATAL_GT (ws, 0, "Could not create waitset: %s\n", dds_strretcode (ws));
  rc = dds_waitset_attach (ws, rdep, 0);
  MPT_ASSERT_FATAL_EQ (rc, 0, "Could not attach built-in reader to waitset: %s\n", dds_strretcode (rc));

  bool done = false;
  bool first = true;
  unsigned exp_index = 0;
  unsigned exp_cycle = 0;
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
      if (si.instance_state != DDS_IST_ALIVE)
        done = true;
      else if (!si.valid_data || strcmp (sample->topic_name, topic_name) != 0)
        continue;
      else
      {
        void *td = NULL;
        size_t tsz = 0;
        if (!dds_qget_topicdata (sample->qos, &td, &tsz))
          printf ("%d: topic data not set in QoS\n", id);
        if (first && tsz == 0)
        {
          dds_qset_topicdata (qos, "X", 1);
          rc = dds_set_qos (tp, qos);
          MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Set QoS failed: %s\n", dds_strretcode (rc));
        }
        else
        {
          const char *exp = exp_ud[exp_index];
          if (first && strcmp (td, "X") == 0)
            exp = "X";
          const size_t expsz = first ? 1 : strlen (exp);
          bool eq = (tsz == expsz && (tsz == 0 || memcmp (td, exp, tsz) == 0));
          //printf ("%d: expected %u %zu/%s received %zu/%s\n",
          //        id, exp_index, expsz, exp, tsz, td ? (char *) td : "(null)");
          MPT_ASSERT (eq, "Topic data mismatch: expected %u %zu/%s received %zu/%s\n",
                      exp_index, expsz, exp, tsz, td ? (char *) td : "(null)");
          if (strcmp (exp, "X") != 0 && ++exp_index == sizeof (exp_ud) / sizeof (exp_ud[0]))
          {
            exp_index = 0;
            exp_cycle++;
          }

          if (active && exp_cycle == ncycles)
            done = true;
          else
          {
            const void *newtd;
            size_t newtsz;
            if (!active)
            {
              /* Set topic data to the same value in response */
              newtd = td; newtsz = tsz;
              dds_qset_topicdata (qos, td, tsz);
            }
            else /* Set next agreed value */
            {
              newtd = exp_ud[exp_index]; newtsz = strlen (exp_ud[exp_index]);
              dds_qset_topicdata (qos, newtd, newtsz);
            }

            rc = dds_set_qos (tp, qos);
            MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Set QoS failed: %s\n", dds_strretcode (rc));

            dds_qos_t *chk = dds_create_qos ();
            rc = dds_get_qos (ep, chk);
            MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Get QoS failed: %s\n", dds_strretcode (rc));

            void *chktd = NULL;
            size_t chktsz = 0;
            if (!dds_qget_topicdata (chk, &chktd, &chktsz))
              MPT_ASSERT (0, "Check QoS: no topic data present\n");
            MPT_ASSERT (chktsz == newtsz && (newtsz == 0 || memcmp (chktd, newtd, newtsz) == 0),
                        "Retrieved topic data differs from topic data just set (%zu/%s vs %zu/%s)\n",
                        chktsz, chktd ? (char *) chktd : "(null)", newtsz, newtd ? (char *) newtd : "(null)");
            dds_free (chktd);
            dds_delete_qos (chk);
            first = false;
          }
        }
        dds_free (td);
      }
    }
    MPT_ASSERT_FATAL_EQ (n, 0, "Read failed: %s\n", dds_strretcode (n));
    dds_return_loan (rdep, &raw, 1);
  }
  dds_delete_qos (qos);
  rc = dds_delete (dp);
  MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "teardown failed\n");
  printf ("=== [Publisher(%d)] Done\n", id);
}

MPT_ProcessEntry (rwgd,
                  MPT_Args (dds_domainid_t domainid,
                            const char *topic_name,
                            bool active,
                            unsigned ncycles))
{
  dds_entity_t dp, tp, ep, rdep, grp, ws;
  dds_return_t rc;
  dds_qos_t *qos;
  int id = (int) ddsrt_getpid ();

  printf ("=== [Check(%d)] active=%d ncycles=%u Start(%d) ...\n", id, active, ncycles, (int) domainid);

  qos = dds_create_qos ();
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dp = dds_create_participant (domainid, NULL, NULL);
  MPT_ASSERT_FATAL_GT (dp, 0, "Could not create participant: %s\n", dds_strretcode (dp));
  tp = dds_create_topic (dp, &RWData_Msg_desc, topic_name, qos, NULL);
  MPT_ASSERT_FATAL_GT (tp, 0, "Could not create topic: %s\n", dds_strretcode (tp));
  if (active)
  {
    rdep = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, qos, NULL);
    MPT_ASSERT_FATAL_GT (rdep, 0, "Could not create DCPSSubscription reader: %s\n", dds_strretcode (rdep));
    ep = dds_create_writer (dp, tp, qos, NULL);
    MPT_ASSERT_FATAL_GT (ep, 0, "Could not create writer: %s\n", dds_strretcode (ep));
  }
  else
  {
    rdep = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, qos, NULL);
    MPT_ASSERT_FATAL_GT (rdep, 0, "Could not create DCPSPublication reader: %s\n", dds_strretcode (rdep));
    ep = dds_create_reader (dp, tp, qos, NULL);
    MPT_ASSERT_FATAL_GT (ep, 0, "Could not create reader: %s\n", dds_strretcode (ep));
  }
  grp = dds_get_parent (ep);
  MPT_ASSERT_FATAL_GT (grp, 0, "Could not get pub/sub from wr/rd: %s\n", dds_strretcode (grp));
  rc = dds_set_status_mask (rdep, DDS_DATA_AVAILABLE_STATUS);
  MPT_ASSERT_FATAL_EQ (rc, 0, "Could not set status mask: %s\n", dds_strretcode (rc));
  ws = dds_create_waitset (dp);
  MPT_ASSERT_FATAL_GT (ws, 0, "Could not create waitset: %s\n", dds_strretcode (ws));
  rc = dds_waitset_attach (ws, rdep, 0);
  MPT_ASSERT_FATAL_EQ (rc, 0, "Could not attach built-in reader to waitset: %s\n", dds_strretcode (rc));

  bool done = false;
  bool first = true;
  unsigned exp_index = 0;
  unsigned exp_cycle = 0;
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
      if (si.instance_state != DDS_IST_ALIVE)
        done = true;
      else if (!si.valid_data || strcmp (sample->topic_name, topic_name) != 0)
        continue;
      else
      {
        void *gd = NULL;
        size_t gsz = 0;
        if (!dds_qget_groupdata (sample->qos, &gd, &gsz))
          printf ("%d: group data not set in QoS\n", id);
        if (first && gsz == 0)
        {
          dds_qset_groupdata (qos, "X", 1);
          rc = dds_set_qos (grp, qos);
          MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Set QoS failed: %s\n", dds_strretcode (rc));
        }
        else
        {
          const char *exp = exp_ud[exp_index];
          if (first && strcmp (gd, "X") == 0)
            exp = "X";
          const size_t expsz = first ? 1 : strlen (exp);
          bool eq = (gsz == expsz && (gsz == 0 || memcmp (gd, exp, gsz) == 0));
          //printf ("%d: expected %u %zu/%s received %zu/%s\n",
          //        id, exp_index, expsz, exp, gsz, gd ? (char *) gd : "(null)");
          MPT_ASSERT (eq, "Group data mismatch: expected %u %zu/%s received %zu/%s\n",
                      exp_index, expsz, exp, gsz, gd ? (char *) gd : "(null)");
          if (strcmp (exp, "X") != 0 && ++exp_index == sizeof (exp_ud) / sizeof (exp_ud[0]))
          {
            exp_index = 0;
            exp_cycle++;
          }

          if (active && exp_cycle == ncycles)
            done = true;
          else
          {
            const void *newgd;
            size_t newgsz;
            if (!active)
            {
              /* Set group data to the same value in response */
              newgd = gd; newgsz = gsz;
              dds_qset_groupdata (qos, gd, gsz);
            }
            else /* Set next agreed value */
            {
              newgd = exp_ud[exp_index]; newgsz = strlen (exp_ud[exp_index]);
              dds_qset_groupdata (qos, newgd, newgsz);
            }

            rc = dds_set_qos (grp, qos);
            MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Set QoS failed: %s\n", dds_strretcode (rc));

            dds_qos_t *chk = dds_create_qos ();
            rc = dds_get_qos (ep, chk);
            MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Get QoS failed: %s\n", dds_strretcode (rc));

            void *chkgd = NULL;
            size_t chkgsz = 0;
            if (!dds_qget_groupdata (chk, &chkgd, &chkgsz))
              MPT_ASSERT (0, "Check QoS: no group data present\n");
            MPT_ASSERT (chkgsz == newgsz && (newgsz == 0 || memcmp (chkgd, newgd, newgsz) == 0),
                        "Retrieved group data differs from group data just set (%zu/%s vs %zu/%s)\n",
                        chkgsz, chkgd ? (char *) chkgd : "(null)", newgsz, newgd ? (char *) newgd : "(null)");
            dds_free (chkgd);
            dds_delete_qos (chk);
            first = false;
          }
        }
        dds_free (gd);
      }
    }
    MPT_ASSERT_FATAL_EQ (n, 0, "Read failed: %s\n", dds_strretcode (n));
    dds_return_loan (rdep, &raw, 1);
  }
  dds_delete_qos (qos);
  rc = dds_delete (dp);
  MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "teardown failed\n");
  printf ("=== [Publisher(%d)] Done\n", id);
}
