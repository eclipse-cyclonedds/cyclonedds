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
#include "dds/ddsrt/log.h"

#include "dds/ddsi/q_xqos.h"

#include "rwdata.h"
#include "rw.h"

#define NPUB 10
#define NWR_PUB 2

void rw_init (void)
{
}

void rw_fini (void)
{
}

static void setqos (dds_qos_t *q, size_t i, bool isrd, bool create)
{
  size_t psi = i - (i % NWR_PUB);
  /* Participant, publisher & topic get created with i == 0,
     so make sure those have some crazy data set.  The writers
     should inherit the topic and group data, but the user data
     should be updated for each writer */
  if (create)
  {
    if (psi == 1)
    {
      dds_qset_userdata (q, NULL, 0);
      dds_qset_topicdata (q, NULL, 0);
      dds_qset_groupdata (q, NULL, 0);
    }
    else
    {
      char buf[20];
      snprintf (buf, sizeof (buf), "ud%zu%c", i, isrd ? 'r' : 'w');
      dds_qset_userdata (q, buf, strlen (buf));
      snprintf (buf, sizeof (buf), "td%zu", i);
      dds_qset_topicdata (q, buf, strlen (buf));
      snprintf (buf, sizeof (buf), "gd%zu", psi);
      dds_qset_groupdata (q, buf, strlen (buf));
    }
  }
  else
  {
    if (psi == 1)
    {
      dds_qset_userdata (q, NULL, 0);
      dds_qset_topicdata (q, NULL, 0);
      dds_qset_groupdata (q, NULL, 0);
    }
    else
    {
      char buf[20];
      snprintf (buf, sizeof (buf), "ud%zu%c", i, isrd ? 'r' : 'w');
      dds_qset_userdata (q, buf, strlen (buf));
      snprintf (buf, sizeof (buf), "td%zu", (size_t) 0);
      dds_qset_topicdata (q, buf, strlen (buf));
      snprintf (buf, sizeof (buf), "gd%zu", psi);
      dds_qset_groupdata (q, buf, strlen (buf));
    }
  }

  /* Cyclone's accepting unimplemented QoS settings is a bug, but it does allow
     us to feed it all kinds of nonsense and see whether discovery manages it */

  /* this makes topic transient-local, keep-last 1 */
  dds_qset_durability (q, (dds_durability_kind_t) ((i + 1) % 4));
  dds_qset_history (q, (dds_history_kind_t) ((i + 1) % 2), (int32_t) (i + 1));
  dds_qset_resource_limits (q, (int32_t) i + 3, (int32_t) i + 2, (int32_t) i + 1);
  dds_qset_presentation (q, (dds_presentation_access_scope_kind_t) ((psi + 1) % 3), 1, 1);
#ifdef DDSI_INCLUDE_LIFESPAN
  dds_qset_lifespan (q, INT64_C (23456789012345678) + (int32_t) i);
#else
  dds_qset_lifespan (q, DDS_INFINITY);
#endif
  dds_qset_deadline (q, INT64_C (67890123456789012) + (int32_t) i);
  dds_qset_latency_budget (q, INT64_C (45678901234567890) + (int32_t) i);
  dds_qset_ownership (q, (dds_ownership_kind_t) ((i + 1) % 2));
  dds_qset_ownership_strength (q, 0x12345670 + (int32_t) i);
  dds_qset_liveliness (q, (dds_liveliness_kind_t) ((i + i) % 2), INT64_C (456789012345678901) + (int32_t) i);
  dds_qset_time_based_filter (q, INT64_C (34567890123456789) + (int32_t) i); /* must be <= deadline */
  if (psi == 0)
    dds_qset_partition1 (q, "p");
  else if (psi == 1)
    dds_qset_partition (q, 0, NULL);
  else
  {
    char **ps = ddsrt_malloc (psi * sizeof (*ps));
    for (size_t j = 0; j < psi; j++)
    {
      const size_t n = 40;
      ps[j] = ddsrt_malloc (n);
      snprintf (ps[j], n, "p%zu_%zu", psi, isrd ? (psi-j-1) : j);
    }
    dds_qset_partition (q, (uint32_t) psi, (const char **) ps);
    for (size_t j = 0; j < psi; j++)
      ddsrt_free (ps[j]);
    ddsrt_free (ps);
  }
  dds_qset_reliability (q, (dds_reliability_kind_t) ((i + 1) % 2), INT64_C (890123456789012345) + (int32_t) i);
  dds_qset_transport_priority (q, 0x23456701 + (int32_t) i);
  dds_qset_destination_order (q, (dds_destination_order_kind_t) ((i + 1) % 2));
  dds_qset_writer_data_lifecycle (q, ((i + 1) % 2) != 0);
  dds_qset_reader_data_lifecycle (q, INT64_C (41234567890123456) + (int32_t) i, INT64_C (912345678901234567) + (int32_t) i);
  dds_qset_durability_service (q, INT64_C (123456789012345678) + (int32_t) i, (dds_history_kind_t) ((i + 1) % 2), (int32_t) (i + 1), (int32_t) i + 3, (int32_t) i + 2, (int32_t) i + 1);
}

static bool pubsub_qos_eq_h (const dds_qos_t *a, dds_entity_t ent)
{
  dds_qos_t *b = dds_create_qos ();
  dds_get_qos (ent, b);
  /* internal interface is more luxurious that a simple compare for equality, and
     using that here saves us a ton of code */
  uint64_t delta = nn_xqos_delta (a, b, QP_GROUP_DATA | QP_PRESENTATION | QP_PARTITION);
  if (delta)
  {
    struct ddsrt_log_cfg logcfg;
    dds_log_cfg_init (&logcfg, 0, DDS_LC_ERROR, stderr, stderr);
    DDS_CLOG (DDS_LC_ERROR, &logcfg, "pub/sub: delta = %"PRIx64"\n", delta);
    nn_log_xqos (DDS_LC_ERROR, &logcfg, a); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
    nn_log_xqos (DDS_LC_ERROR, &logcfg, b); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
  }
  dds_delete_qos (b);
  return delta == 0;
}

static uint64_t reader_qos_delta (const dds_qos_t *a, const dds_qos_t *b)
{
  return nn_xqos_delta (a, b, QP_USER_DATA | QP_TOPIC_DATA | QP_GROUP_DATA | QP_DURABILITY | QP_HISTORY | QP_RESOURCE_LIMITS | QP_PRESENTATION | QP_DEADLINE | QP_LATENCY_BUDGET | QP_OWNERSHIP | QP_LIVELINESS | QP_TIME_BASED_FILTER | QP_PARTITION | QP_RELIABILITY | QP_DESTINATION_ORDER | QP_PRISMTECH_READER_DATA_LIFECYCLE);
}

static bool reader_qos_eq_h (const dds_qos_t *a, dds_entity_t ent)
{
  dds_qos_t *b = dds_create_qos ();
  dds_get_qos (ent, b);
  uint64_t delta = reader_qos_delta (a, b);
  if (delta)
  {
    struct ddsrt_log_cfg logcfg;
    dds_log_cfg_init (&logcfg, 0, DDS_LC_ERROR, stderr, stderr);
    DDS_CLOG (DDS_LC_ERROR, &logcfg, "reader: delta = %"PRIx64"\n", delta);
    nn_log_xqos (DDS_LC_ERROR, &logcfg, a); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
    nn_log_xqos (DDS_LC_ERROR, &logcfg, b); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
  }
  dds_delete_qos (b);
  return delta == 0;
}

static uint64_t writer_qos_delta (const dds_qos_t *a, const dds_qos_t *b)
{
  return nn_xqos_delta (a, b, QP_USER_DATA | QP_TOPIC_DATA | QP_GROUP_DATA | QP_DURABILITY | QP_HISTORY | QP_RESOURCE_LIMITS | QP_PRESENTATION | QP_LIFESPAN | QP_DEADLINE | QP_LATENCY_BUDGET | QP_OWNERSHIP | QP_OWNERSHIP_STRENGTH | QP_LIVELINESS | QP_PARTITION | QP_RELIABILITY | QP_DESTINATION_ORDER | QP_PRISMTECH_WRITER_DATA_LIFECYCLE);
}

static bool writer_qos_eq_h (const dds_qos_t *a, dds_entity_t ent)
{
  dds_qos_t *b = dds_create_qos ();
  dds_get_qos (ent, b);
  uint64_t delta = writer_qos_delta (a, b);
  if (delta)
  {
    struct ddsrt_log_cfg logcfg;
    dds_log_cfg_init (&logcfg, 0, DDS_LC_ERROR, stderr, stderr);
    DDS_CLOG (DDS_LC_ERROR, &logcfg, "writer: delta = %"PRIx64"\n", delta);
    nn_log_xqos (DDS_LC_ERROR, &logcfg, a); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
    nn_log_xqos (DDS_LC_ERROR, &logcfg, b); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
  }
  dds_delete_qos (b);
  return delta == 0;
}

#define UD_QMPUB "qosmatch_publisher"
#define UD_QMPUBDONE UD_QMPUB ":ok"

MPT_ProcessEntry (rw_publisher,
                  MPT_Args (dds_domainid_t domainid,
                            const char *topic_name))
{
  dds_entity_t dp;
  dds_entity_t tp;
  dds_entity_t pub[NPUB];
  dds_entity_t wr[NPUB][NWR_PUB];
  bool chk[NPUB][NWR_PUB] = { { false } };
  dds_return_t rc;
  dds_qos_t *qos, *ppqos;
  int id = (int) ddsrt_getpid ();

  printf ("=== [Publisher(%d)] Start(%d) ...\n", id, (int) domainid);

  ppqos = dds_create_qos ();
  dds_qset_userdata (ppqos, UD_QMPUB, sizeof (UD_QMPUB) - 1);
  dp = dds_create_participant (domainid, ppqos, NULL);
  MPT_ASSERT_FATAL_GT (dp, 0, "Could not create participant: %s\n", dds_strretcode (dp));

  qos = dds_create_qos ();
  setqos (qos, 0, false, true);
  tp = dds_create_topic (dp, &RWData_Msg_desc, topic_name, qos, NULL);
  MPT_ASSERT_FATAL_GT (tp, 0, "Could not create topic: %s\n", dds_strretcode (tp));

  for (size_t i = 0; i < NPUB; i++)
  {
    setqos (qos, i * NWR_PUB, false, true);
    pub[i] = dds_create_publisher (dp, qos, NULL);
    MPT_ASSERT_FATAL_GT (pub[i], 0, "Could not create publisher %zu: %s\n", i, dds_strretcode (pub[i]));
    for (size_t j = 0; j < NWR_PUB; j++)
    {
      setqos (qos, i * NWR_PUB + j, false, true);
      wr[i][j] = dds_create_writer (pub[i], tp, qos, NULL);
      MPT_ASSERT_FATAL_GT (wr[i][j], 0, "Could not create writer %zu %zu: %s\n", i, j, dds_strretcode (wr[i][j]));
    }
  }

  for (size_t i = 0; i < NPUB; i++)
  {
    setqos (qos, i * NWR_PUB, false, false);
    MPT_ASSERT (pubsub_qos_eq_h (qos, pub[i]), "publisher %zu QoS mismatch\n", i);
    for (size_t j = 0; j < NWR_PUB; j++)
    {
      setqos (qos, i * NWR_PUB + j, false, false);
      MPT_ASSERT (writer_qos_eq_h (qos, wr[i][j]), "writer %zu %zu QoS mismatch\n", i, j);
    }
  }

  /* Each writer should match exactly one reader */
  uint32_t nchk = 0;
  while (nchk != NPUB * NWR_PUB)
  {
    for (size_t i = 0; i < NPUB; i++)
    {
      for (size_t j = 0; j < NWR_PUB; j++)
      {
        if (chk[i][j])
          continue;
        dds_instance_handle_t ih;
        dds_builtintopic_endpoint_t *ep;
        rc = dds_get_matched_subscriptions (wr[i][j], &ih, 1);
        MPT_ASSERT (rc == 0 || rc == 1, "Unexpected return from get_matched_subscriptions for writer %zu %zu: %s\n",
                    i, j, dds_strretcode (rc));
        if (rc == 1)
        {
          ep = dds_get_matched_subscription_data (wr[i][j], ih);
          MPT_ASSERT (ep != NULL, "Failed to retrieve matched subscription data for writer %zu %zu\n", i, j);
          setqos (qos, i * NWR_PUB + j, true, false);
          uint64_t delta = reader_qos_delta (qos, ep->qos);
          if (delta)
          {
            struct ddsrt_log_cfg logcfg;
            dds_log_cfg_init (&logcfg, 0, DDS_LC_ERROR, stderr, stderr);
            DDS_CLOG (DDS_LC_ERROR, &logcfg, "matched reader: delta = %"PRIx64"\n", delta);
            nn_log_xqos (DDS_LC_ERROR, &logcfg, qos); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
            nn_log_xqos (DDS_LC_ERROR, &logcfg, ep->qos); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
          }
          MPT_ASSERT (delta == 0, "writer %zu %zu matched reader QoS mismatch\n", i, j);
          dds_delete_qos (ep->qos);
          dds_free (ep->topic_name);
          dds_free (ep->type_name);
          dds_free (ep);
          chk[i][j] = true;
          nchk++;
        }
      }
    }
    dds_sleepfor (DDS_MSECS (100));
  }

  dds_qset_userdata (ppqos, UD_QMPUBDONE, sizeof (UD_QMPUBDONE) - 1);
  rc = dds_set_qos (dp, ppqos);
  MPT_ASSERT_FATAL_EQ (rc, DDS_RETCODE_OK, "failed to participant QoS: %s\n", dds_strretcode (rc));

  /* Wait until subscribers terminate */
  printf ("wait for subscribers to terminate\n");
  fflush (stdout);
  while (true)
  {
    for (size_t i = 0; i < NPUB; i++)
    {
      for (size_t j = 0; j < NWR_PUB; j++)
      {
        dds_publication_matched_status_t st;
        rc = dds_get_publication_matched_status (wr[i][j], &st);
        MPT_ASSERT_FATAL_EQ (rc, DDS_RETCODE_OK, "dds_get_matched_publication_status failed for writer %zu %zu: %s\n",
                             i, j, dds_strretcode (rc));
        if (st.current_count)
        {
          goto have_matches;
        }
      }
    }
    break;
  have_matches:
    ;
  }

  dds_delete_qos (qos);
  dds_delete_qos (ppqos);
  rc = dds_delete (dp);
  MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "teardown failed\n");
  printf ("=== [Publisher(%d)] Done\n", id);
}

static void wait_for_done (dds_entity_t rd, const char *userdata)
{
  int32_t n;
  void *raw = NULL;
  dds_sample_info_t si;
  bool done = false;
  while (!done)
  {
    while (!done && (n = dds_take (rd, &raw, &si, 1, 1)) == 1)
    {
      const dds_builtintopic_participant_t *sample = raw;
      void *ud = NULL;
      size_t usz = 0;
      if (!si.valid_data || !dds_qget_userdata (sample->qos, &ud, &usz))
        continue;
      if (ud && strcmp (ud, userdata) == 0)
        done = true;
      dds_free (ud);
      dds_return_loan (rd, &raw, 1);
    }

    if (!done)
      dds_sleepfor (DDS_MSECS (100));
  }
}

MPT_ProcessEntry (rw_subscriber,
                  MPT_Args (dds_domainid_t domainid,
                            const char *topic_name))
{
  dds_entity_t dp, pprd;
  dds_entity_t tp;
  dds_entity_t sub[NPUB];
  dds_entity_t rd[NPUB][NWR_PUB];
  bool chk[NPUB][NWR_PUB] = { { false } };
  dds_return_t rc;
  dds_qos_t *qos;
  int id = (int) ddsrt_getpid ();

  printf ("=== [Subscriber(%d)] Start(%d) ...\n", id, (int) domainid);

  dp = dds_create_participant (domainid, NULL, NULL);
  MPT_ASSERT_FATAL_GT (dp, 0, "Could not create participant: %s\n", dds_strretcode (dp));
  pprd = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);
  MPT_ASSERT_FATAL_GT (pprd, 0, "Could not create DCPSParticipant reader: %s\n", dds_strretcode (pprd));

  qos = dds_create_qos ();
  setqos (qos, 0, true, true);
  tp = dds_create_topic (dp, &RWData_Msg_desc, topic_name, qos, NULL);
  MPT_ASSERT_FATAL_GT (tp, 0, "Could not create topic: %s\n", dds_strretcode (tp));

  for (size_t i = 0; i < NPUB; i++)
  {
    setqos (qos, i * NWR_PUB, true, true);
    sub[i] = dds_create_subscriber (dp, qos, NULL);
    MPT_ASSERT_FATAL_GT (sub[i], 0, "Could not create subscriber %zu: %s\n", i, dds_strretcode (sub[i]));
    for (size_t j = 0; j < NWR_PUB; j++)
    {
      setqos (qos, i * NWR_PUB + j, true, true);
      rd[i][j] = dds_create_reader (sub[i], tp, qos, NULL);
      MPT_ASSERT_FATAL_GT (rd[i][j], 0, "Could not create reader %zu %zu: %s\n", i, j, dds_strretcode (rd[i][j]));
    }
  }

  for (size_t i = 0; i < NPUB; i++)
  {
    setqos (qos, i * NWR_PUB, true, false);
    MPT_ASSERT (pubsub_qos_eq_h (qos, sub[i]), "subscriber %zu QoS mismatch\n", i);
    for (size_t j = 0; j < NWR_PUB; j++)
    {
      setqos (qos, i * NWR_PUB + j, true, false);
      MPT_ASSERT (reader_qos_eq_h (qos, rd[i][j]), "reader %zu %zu QoS mismatch\n", i, j);
    }
  }

  /* Each writer should match exactly one reader */
  uint32_t nchk = 0;
  while (nchk != NPUB * NWR_PUB)
  {
    for (size_t i = 0; i < NPUB; i++)
    {
      for (size_t j = 0; j < NWR_PUB; j++)
      {
        if (chk[i][j])
          continue;
        dds_instance_handle_t ih;
        dds_builtintopic_endpoint_t *ep;
        rc = dds_get_matched_publications (rd[i][j], &ih, 1);
        MPT_ASSERT (rc == 0 || rc == 1, "Unexpected return from get_matched_publications for writer %zu %zu: %s\n",
                    i, j, dds_strretcode (rc));
        if (rc == 1)
        {
          ep = dds_get_matched_publication_data (rd[i][j], ih);
          MPT_ASSERT (ep != NULL, "Failed to retrieve matched publication data for writer %zu %zu\n", i, j);
          setqos (qos, i * NWR_PUB + j, false, false);
          uint64_t delta = writer_qos_delta (qos, ep->qos);
          if (delta)
          {
            struct ddsrt_log_cfg logcfg;
            dds_log_cfg_init (&logcfg, 0, DDS_LC_ERROR, stderr, stderr);
            DDS_CLOG (DDS_LC_ERROR, &logcfg, "matched writer: delta = %"PRIx64"\n", delta);
            nn_log_xqos (DDS_LC_ERROR, &logcfg, qos); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
            nn_log_xqos (DDS_LC_ERROR, &logcfg, ep->qos); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
          }
          MPT_ASSERT (delta == 0, "reader %zu %zu matched writer QoS mismatch\n", i, j);
          dds_delete_qos (ep->qos);
          dds_free (ep->topic_name);
          dds_free (ep->type_name);
          dds_free (ep);
          chk[i][j] = true;
          nchk++;
        }
      }
    }
    dds_sleepfor (DDS_MSECS (100));
  }

  printf ("wait for publisher to have completed its checks\n");
  wait_for_done (pprd, UD_QMPUBDONE);

  dds_delete_qos (qos);
  rc = dds_delete (dp);
  MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "teardown failed\n");
  printf ("=== [Subscriber(%d)] Done\n", id);
}
